#define GL_GLEXT_PROTOTYPES

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <wayland-client.h>
#include <wayland-egl.h>

#include <EGL/egl.h>
#include <GL/gl.h>
#include <chrono>
#include <cmath>
#include <unordered_map>
#include <string>
#include <fstream>
#include <sstream>
#include <cstdarg>
#include "xdg-shell.h"

static const char *default_vert = R"(
#version 120
void main() {
    gl_Position = gl_Vertex;
}
)";

static const char *vertex_shader_src;

static const char *default_frag = R"(
// Perfect circle test
#version 120
uniform vec2  u_resolution;
uniform float u_time;

void main() {
    // Get pixel coordinates
    vec2 fragCoord = gl_FragCoord.xy;

    // Center point in pixels
    vec2 center = u_resolution * 0.5;

    // Distance in pixels (preserves aspect ratio)
    float dist = distance(fragCoord, center);

    // Circle radius in pixels (so it's always round)
    float radius = 50.0;

    if (dist < radius) {
        // Pink color
        vec3 color = vec3(1.0, 0.4, 0.7);
        gl_FragColor = vec4(color, 0.9);
    } else {
        gl_FragColor = vec4(0.0);
    }
}
)";
#pragma region Settings
struct Settings {
    std::unordered_map<std::string, std::string> kv;

    bool load(const std::string& path) {
        std::ifstream f(path);
        if (!f) return false;

        std::string line;
        while (std::getline(f, line)) {
            if (line.empty()) continue;

            auto eq = line.find('=');
            if (eq == std::string::npos) continue;

            std::string key = line.substr(0, eq);
            std::string val = line.substr(eq + 1);

            trim(key);
            trim(val);

            kv[key] = val;
        }
        return true;
    }

    bool save(const std::string& path) const {
        std::ofstream f(path);
        if (!f) return false;

        for (auto& [k, v] : kv)
            f << k << " = " << v << "\n";

        return true;
    }

    int get_int(const std::string& key, int def) const {
        auto it = kv.find(key);
        if (it == kv.end()) return def;
        return std::stoi(it->second);
    }

    std::string get_string(const std::string& key,
                           const std::string& def) const {
        auto it = kv.find(key);
        if (it == kv.end()) return def;
        return it->second;
    }

    void set_int(const std::string& key, int v) {
        kv[key] = std::to_string(v);
    }

    void set_string(const std::string& key, const std::string& v) {
        kv[key] = v;
    }

private:
    static void trim(std::string& s) {
        const char* ws = " \t\n\r";
        s.erase(0, s.find_first_not_of(ws));
        s.erase(s.find_last_not_of(ws) + 1);
    }
};
#pragma endregion
static bool debug = false;
void logDebug(const char* format, ...) {
    if (!debug) return;
    
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

static const char *fragment_shader_src;
static const char *fragment_shader2_src;
struct client_state {
  Settings settings;
  int setting_wrap_t = 0;
  int setting_wrap_s = 0;
  
  wl_display *display;
  wl_registry *registry;
  wl_compositor *compositor;

  xdg_wm_base *wm_base;
  wl_surface *surface;
  xdg_surface *xdg_surface_obj;
  xdg_toplevel *toplevel;

  wl_egl_window *egl_window;
  EGLDisplay egl_display;
  EGLConfig egl_config;
  EGLContext egl_context;
  EGLSurface egl_surface;

  int width;
  int height;
  bool running;
  int frame_count = 0;

  GLuint shader_prog;
  GLint u_resolution;
  GLint u_time;
  GLint u_frame;
  GLuint fbo[2];
  GLuint texture[2];
  int current_buffer;
  bool base_multipass = false;

  GLuint layer1_prog;
  GLint layer1_u_resolution, layer1_u_time, layer1_u_frame;
  GLuint layer1_fbo[2], layer1_texture[2];
  int layer1_current_buffer;
  bool layer1_multipass = false;
  bool layer1_enabled;

  GLuint quad_vao, quad_vbo;
};

static void init_multipass(client_state *st) {
  // Generate framebuffers and textures
  glGenFramebuffers(2, st->fbo);
  glGenTextures(2, st->texture);

  for (int i = 0; i < 2; i++) {
    // Setup texture
    glBindTexture(GL_TEXTURE_2D, st->texture[i]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, st->width, st->height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, st->setting_wrap_s);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, st->setting_wrap_t);

    // Attach texture to framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, st->fbo[i]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           st->texture[i], 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      logDebug("Framebuffer %d not complete!", i);
    }
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  st->current_buffer = 0;

  // Get uniform location for texture sampler
  glUseProgram(st->shader_prog);
  GLint base_channel0 = glGetUniformLocation(st->shader_prog, "iChannel0");
  if (base_channel0 != -1) {
    glUniform1i(base_channel0, 0);
  }

  if (st->layer1_multipass) {
    glGenFramebuffers(2, st->layer1_fbo);
    glGenTextures(2, st->layer1_texture);

    for (int i = 0; i < 2; i++) {
      glBindTexture(GL_TEXTURE_2D, st->layer1_texture[i]);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, st->width, st->height, 0, GL_RGBA,
                   GL_UNSIGNED_BYTE, NULL);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, st->setting_wrap_s);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, st->setting_wrap_t);

      glBindFramebuffer(GL_FRAMEBUFFER, st->layer1_fbo[i]);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D, st->layer1_texture[i], 0);

      if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        logDebug("Layer1 framebuffer %d not complete!", i);
      }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    st->layer1_current_buffer = 0;

    glUseProgram(st->layer1_prog);
    GLint layer1_channel0 = glGetUniformLocation(st->layer1_prog, "iChannel0");
    if (layer1_channel0 != -1) {
      glUniform1i(layer1_channel0, 0);
    }
    GLint layer1_channel1 = glGetUniformLocation(st->layer1_prog, "iChannel1");
    if (layer1_channel1 != -1) {
      glUniform1i(layer1_channel1, 1);
    }

    logDebug("Layer1 multipass initialized");
  }
}

static void resize_multipass(client_state *st) {
  // Delete old textures
  glDeleteTextures(2, st->texture);
  if (st->layer1_multipass) {
    glDeleteTextures(2, st->layer1_texture);
  }

  // My textures
  glGenTextures(2, st->texture);

  for (int i = 0; i < 2; i++) {
    glBindTexture(GL_TEXTURE_2D, st->texture[i]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, st->width, st->height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, st->setting_wrap_s);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, st->setting_wrap_t);

    // Reattach to framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, st->fbo[i]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           st->texture[i], 0);
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  logDebug("Resized FBO textures to %dx%d", st->width, st->height);

  if (st->layer1_multipass) {
    glGenTextures(2, st->layer1_texture);

    for (int i = 0; i < 2; i++) {
      glBindTexture(GL_TEXTURE_2D, st->layer1_texture[i]);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, st->width, st->height, 0, GL_RGBA,
                   GL_UNSIGNED_BYTE, NULL);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, st->setting_wrap_s);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, st->setting_wrap_t);

      // Reattach to framebuffer
      glBindFramebuffer(GL_FRAMEBUFFER, st->layer1_fbo[i]);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D, st->layer1_texture[i], 0);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    logDebug("Resized layer1 FBO textures to %dx%d", st->width, st->height);
  }
}

std::string load_shader_from_file(const std::string &filepath) {
  std::ifstream file(filepath);
  if (!file.is_open()) {
    return "";
  }
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

double get_time_seconds() {
  using clock = std::chrono::high_resolution_clock;
  static auto start_time = clock::now();
  auto now = clock::now();
  std::chrono::duration<double> elapsed = now - start_time;
  return elapsed.count();
}

static void registry_global(void *data, wl_registry *registry, uint32_t name,
                            const char *interface, uint32_t version) {
  client_state *st = (client_state *)data;

  if (strcmp(interface, wl_compositor_interface.name) == 0) {
    st->compositor = (wl_compositor *)wl_registry_bind(
        registry, name, &wl_compositor_interface, 4);
  } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
    st->wm_base = (xdg_wm_base *)wl_registry_bind(registry, name,
                                                  &xdg_wm_base_interface, 1);
  }
}

static void registry_remove(void *, wl_registry *, uint32_t) {}

static const wl_registry_listener registry_listener = {registry_global,
                                                       registry_remove};

// Shell listeners
static void wm_base_ping(void *, xdg_wm_base *base, uint32_t serial) {
  xdg_wm_base_pong(base, serial);
}

static const xdg_wm_base_listener wm_base_listener = {wm_base_ping};

static void xdg_surface_configure(void *, xdg_surface *surface,
                                  uint32_t serial) {
  xdg_surface_ack_configure(surface, serial);
}

static const xdg_surface_listener xdg_surface_listener = {
    xdg_surface_configure};

static void toplevel_configure(void *data, xdg_toplevel *, int32_t width,
                               int32_t height, wl_array *) {
  client_state *st = (client_state *)data;

  if (width > 0 && height > 0) {
    st->width = width;
    st->height = height;
    // logDebug("Window resize");
    resize_multipass(st);
    wl_egl_window_resize(st->egl_window, width, height, 0, 0);
  }
}

static void toplevel_close(void *data, xdg_toplevel *) {
  ((client_state *)data)->running = false;
}

static const xdg_toplevel_listener toplevel_listener = {toplevel_configure,
                                                        toplevel_close};

// EGL
static void init_egl(client_state *st) {
  st->egl_display = eglGetDisplay((EGLNativeDisplayType)st->display);
  eglInitialize(st->egl_display, nullptr, nullptr);

  const EGLint cfg[] = {EGL_SURFACE_TYPE,
                        EGL_WINDOW_BIT,
                        EGL_RENDERABLE_TYPE,
                        EGL_OPENGL_BIT,
                        EGL_RED_SIZE,
                        8,
                        EGL_GREEN_SIZE,
                        8,
                        EGL_BLUE_SIZE,
                        8,
                        EGL_ALPHA_SIZE,
                        8,
                        EGL_NONE};

  EGLint count;
  eglChooseConfig(st->egl_display, cfg, &st->egl_config, 1, &count);

  eglBindAPI(EGL_OPENGL_API);
  st->egl_context = eglCreateContext(st->egl_display, st->egl_config,
                                     EGL_NO_CONTEXT, nullptr);

  st->egl_window = wl_egl_window_create(st->surface, st->width, st->height);

  st->egl_surface =
      eglCreateWindowSurface(st->egl_display, st->egl_config,
                             (EGLNativeWindowType)st->egl_window, nullptr);

  eglMakeCurrent(st->egl_display, st->egl_surface, st->egl_surface,
                 st->egl_context);
}

static GLuint compile_shader(GLenum type, const char *src) {
  GLuint s = glCreateShader(type);
  glShaderSource(s, 1, &src, nullptr);
  glCompileShader(s);

  GLint success;
  glGetShaderiv(s, GL_COMPILE_STATUS, &success);
  if (!success) {
    char log[512];
    glGetShaderInfoLog(s, 512, NULL, log);
    logDebug("Shader compilation failed:\n%s", log);
  } else {
    logDebug("Shader comp success");
  }
  return s;
}

static GLuint create_program(const char *frag_shader) {
  GLuint vs = compile_shader(GL_VERTEX_SHADER, vertex_shader_src);
  GLuint fs = compile_shader(GL_FRAGMENT_SHADER, frag_shader);

  GLuint prog = glCreateProgram();
  glAttachShader(prog, vs);
  glAttachShader(prog, fs);
  glLinkProgram(prog);

  glDeleteShader(vs);
  glDeleteShader(fs);
  return prog;
}

static void draw(client_state *st) {
  static double start = get_time_seconds();
  float t = (float)(get_time_seconds() - start);

  auto draw_fullscreen_quad = [](int with_texcoords) {
    glBegin(GL_TRIANGLE_STRIP);

    if (with_texcoords) {
      glTexCoord2f(0.f, 0.f);
      glVertex2f(-1.f, -1.f);
      glTexCoord2f(1.f, 0.f);
      glVertex2f(1.f, -1.f);
      glTexCoord2f(0.f, 1.f);
      glVertex2f(-1.f, 1.f);
      glTexCoord2f(1.f, 1.f);
      glVertex2f(1.f, 1.f);
    } else {
      glVertex2f(-1.f, -1.f);
      glVertex2f(1.f, -1.f);
      glVertex2f(-1.f, 1.f);
      glVertex2f(1.f, 1.f);
    }

    glEnd();
  };
  
  // Base multi
  if (st->base_multipass) {
    int read_buffer = st->current_buffer;
    int write_buffer = 1 - st->current_buffer;

    glUseProgram(st->shader_prog);

    glBindFramebuffer(GL_FRAMEBUFFER, st->fbo[write_buffer]);
    glViewport(0, 0, st->width, st->height);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, st->texture[read_buffer]);

    glUniform2f(st->u_resolution, st->width, st->height);
    glUniform1f(st->u_time, t);
    glUniform1f(st->u_frame, st->frame_count);

    draw_fullscreen_quad(0);

    st->current_buffer = write_buffer;
  }

  // Layer 1 multi
  if (st->layer1_enabled && st->layer1_multipass) {
    int read_buffer = st->layer1_current_buffer;
    int write_buffer = 1 - st->layer1_current_buffer;

    glUseProgram(st->layer1_prog);

    glBindFramebuffer(GL_FRAMEBUFFER, st->layer1_fbo[write_buffer]);
    glViewport(0, 0, st->width, st->height);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, st->texture[st->current_buffer]);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, st->layer1_texture[read_buffer]);

    glUniform2f(st->layer1_u_resolution, st->width, st->height);
    glUniform1f(st->layer1_u_time, t);
    glUniform1f(st->layer1_u_frame, st->frame_count);

    draw_fullscreen_quad(0);

    st->layer1_current_buffer = write_buffer;
  }

  // Composite
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, st->width, st->height);
  glClearColor(0.f, 0.f, 0.f, 0.f);
  glClear(GL_COLOR_BUFFER_BIT);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_TEXTURE_2D);

  // Base
  glUseProgram(st->shader_prog);
  if (st->base_multipass) {
    glBindTexture(GL_TEXTURE_2D, st->texture[st->current_buffer]);

    draw_fullscreen_quad(1);
  } else {
    glUniform2f(st->u_resolution, st->width, st->height);
    glUniform1f(st->u_time, t);
    glUniform1f(st->u_frame, st->frame_count);

    draw_fullscreen_quad(0);
  }

  // Layer 1
  if (st->layer1_enabled) {
    glUseProgram(st->layer1_prog);

    if (st->layer1_multipass) {
      glBindTexture(GL_TEXTURE_2D,
                    st->layer1_texture[st->layer1_current_buffer]);

      draw_fullscreen_quad(1);
    } else {
      glUniform2f(st->layer1_u_resolution, st->width, st->height);
      glUniform1f(st->layer1_u_time, t);
      glUniform1f(st->layer1_u_frame, st->frame_count);

      draw_fullscreen_quad(0);
    }
  }

  glDisable(GL_TEXTURE_2D);
  glDisable(GL_BLEND);

  GLenum err;
  while ((err = glGetError()) != GL_NO_ERROR) {
    logDebug("OpenGL error: 0x%x", err);
  }

  eglSwapBuffers(st->egl_display, st->egl_surface);
  st->frame_count++;
}

int main() {
  std::string programClass = "";
  bool customShader = false;
  // TODO: this is so messy
  std::string userShader = load_shader_from_file("shader.frag");
  std::string userVert = load_shader_from_file("shader.vert");
  if (!userShader.empty()) {
    customShader = true;
    fragment_shader_src = userShader.c_str();

    if (!userVert.empty()) {
      vertex_shader_src = userVert.c_str();
    } else {
      logDebug("It's recommended you provide your own vertex file (shader.vert) "
             "- using default");
      vertex_shader_src = default_vert;
    }
  } else {
    logDebug("No file 'shader.frag' next to program found - displaying default "
           "test");
    fragment_shader_src = default_frag;
    vertex_shader_src = default_vert;
  }

  client_state st{};
  st.settings = Settings();
  if(!st.settings.load("wayshaders.conf")){
    st.settings.set_int("debug", 0);
    st.settings.set_string("class", "wgts");
    st.settings.set_int("wrap_t", GL_REPEAT);
    st.settings.set_int("wrap_s", GL_REPEAT);
    st.settings.save("wayshaders.conf");
  }else{
    debug = st.settings.get_int("debug", 0);
    programClass = st.settings.get_string("class", "wgts");
    st.setting_wrap_t = st.settings.get_int("wrap_t", GL_CLAMP_TO_EDGE);
    st.setting_wrap_s = st.settings.get_int("wrap_s", GL_CLAMP_TO_EDGE);
  }
  st.width = 800;
  st.height = 600;
  st.running = true;

  st.display = wl_display_connect(nullptr);
  st.registry = wl_display_get_registry(st.display);
  wl_registry_add_listener(st.registry, &registry_listener, &st);
  wl_display_roundtrip(st.display);

  xdg_wm_base_add_listener(st.wm_base, &wm_base_listener, nullptr);

  st.surface = wl_compositor_create_surface(st.compositor);

  st.xdg_surface_obj = xdg_wm_base_get_xdg_surface(st.wm_base, st.surface);

  xdg_surface_add_listener(st.xdg_surface_obj, &xdg_surface_listener, nullptr);

  st.toplevel = xdg_surface_get_toplevel(st.xdg_surface_obj);

  xdg_toplevel_set_title(st.toplevel, "wgts");
  xdg_toplevel_set_app_id(st.toplevel, programClass.c_str());
  xdg_toplevel_add_listener(st.toplevel, &toplevel_listener, &st);

  wl_surface_commit(st.surface);

  init_egl(&st);
  st.shader_prog = create_program(fragment_shader_src);
  logDebug("shader_prog created: %u", st.shader_prog);

  GLint channel_base = glGetUniformLocation(st.shader_prog, "iChannel0");
  if (channel_base != -1) {
    logDebug("base - Multipass on");
    st.base_multipass = true;
  } else {
    logDebug("base - multipass off");
  }
  st.u_resolution = glGetUniformLocation(st.shader_prog, "u_resolution");
  st.u_time = glGetUniformLocation(st.shader_prog, "u_time");
  st.u_frame = glGetUniformLocation(st.shader_prog, "u_frame");
  logDebug("base uniforms: u_resolution=%d, u_time=%d u_frame=%d",
         st.u_resolution, st.u_time, st.u_frame);

  // Load additional shaders
  std::string test = "";
  if (customShader) {
    test = load_shader_from_file("shader2.frag");
    if (test.length() < 2) {
      logDebug("No shader2 / layer1 found");
    } else {
      fragment_shader2_src = test.c_str();
      logDebug("Shader2 / layer1 loaded -> compiling");
      st.layer1_enabled = true;
      st.layer1_prog = create_program(fragment_shader2_src);
      GLint channel_layer1 = glGetUniformLocation(st.layer1_prog, "iChannel1");
      GLint channel_base_for_layer1 =
          glGetUniformLocation(st.layer1_prog, "iChannel0");

      if (channel_layer1 != -1) {
        logDebug("layer1 - Multipass on");
        st.layer1_multipass = true;
      } else {
        logDebug("layer1 - multipass off");
      }
      logDebug("layer1_prog created: %u", st.layer1_prog);
      st.layer1_u_resolution =
          glGetUniformLocation(st.layer1_prog, "u_resolution");
      st.layer1_u_time = glGetUniformLocation(st.layer1_prog, "u_time");
      st.layer1_u_frame = glGetUniformLocation(st.layer1_prog, "u_frame");
      logDebug("layer1 uniforms: u_resolution=%d, u_time=%d u_frame=%d",
             st.layer1_u_resolution, st.layer1_u_time, st.layer1_u_frame);
    }
  }

  init_multipass(&st);

  GLint success;
  glGetProgramiv(st.shader_prog, GL_LINK_STATUS, &success);
  if (!success) {
    char log[512];
    glGetProgramInfoLog(st.shader_prog, 512, NULL, log);
    if (st.layer1_enabled)
      glGetProgramInfoLog(st.layer1_prog, 512, NULL, log);

    logDebug("Shader linking failed:\n%s", log);
  } else {
    logDebug("Shader link success");
  }
  while (st.running) {
    wl_display_dispatch_pending(st.display);
    draw(&st);
  }

  eglDestroySurface(st.egl_display, st.egl_surface);
  eglDestroyContext(st.egl_display, st.egl_context);
  wl_egl_window_destroy(st.egl_window);

  xdg_toplevel_destroy(st.toplevel);
  xdg_surface_destroy(st.xdg_surface_obj);
  wl_surface_destroy(st.surface);
  wl_display_disconnect(st.display);

  return 0;
}
