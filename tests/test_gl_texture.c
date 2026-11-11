/* Texture allocation failures and byte strides, without a GL context. */
#include "../src/cui_internal.h"

static bool fail_alloc;
static void *texture_realloc(void *p, size_t bytes) { return fail_alloc ? NULL : realloc(p, bytes); }
#define realloc texture_realloc
#include "../src/render/cui_render_gl.c"
#undef realloc

static int failed, uploads;
#define CHECK(c) do { if (!(c)) { failed++; fprintf(stderr, "FAIL %d: %s\n", __LINE__, #c); } } while (0)
static const void *upload_pixels[4];
static int upload_y[4], upload_height[4];
static GLenum_ failed_shader;
static int shader_count;
static const unsigned char *CUI_GLAPI shader_version(GLenum_ name) { (void)name; return (const unsigned char *)"3.3"; }
static GLuint_ CUI_GLAPI create_shader(GLenum_ type) { shader_count++; return type; }
static void CUI_GLAPI shader_source(GLuint_ id, GLsizei_ count, const GLchar_ *const *src, const GLint_ *len) { (void)id; (void)count; (void)src; (void)len; }
static void CUI_GLAPI compile_shader(GLuint_ id) { (void)id; }
static void CUI_GLAPI shader_status(GLuint_ id, GLenum_ name, GLint_ *ok) { (void)name; *ok = id != failed_shader; }
static void CUI_GLAPI shader_log(GLuint_ id, GLsizei_ cap, GLsizei_ *len, GLchar_ *out) { (void)id; (void)len; if (cap > 0) out[0] = 0; }
static void CUI_GLAPI delete_shader(GLuint_ id) { (void)id; shader_count--; }
static void CUI_GLAPI unused_gl(void) { CHECK(false); }
static void *shader_loader(const char *name) {
    if (!strcmp(name, "glGetString")) return (void *)shader_version;
    if (!strcmp(name, "glCreateShader")) return (void *)create_shader;
    if (!strcmp(name, "glShaderSource")) return (void *)shader_source;
    if (!strcmp(name, "glCompileShader")) return (void *)compile_shader;
    if (!strcmp(name, "glGetShaderiv")) return (void *)shader_status;
    if (!strcmp(name, "glGetShaderInfoLog")) return (void *)shader_log;
    if (!strcmp(name, "glDeleteShader")) return (void *)delete_shader;
    return (void *)unused_gl;
}
static void CUI_GLAPI gen_textures(GLsizei_ n, GLuint_ *out) { for (int i = 0; i < n; i++) out[i] = 1; }
static void CUI_GLAPI bind_texture(GLenum_ target, GLuint_ id) { (void)target; (void)id; }
static void CUI_GLAPI tex_parameter(GLenum_ target, GLenum_ param, GLint_ value) { (void)target; (void)param; (void)value; }
static void CUI_GLAPI pixel_store(GLenum_ param, GLint_ value) { (void)param; (void)value; }
static void CUI_GLAPI tex_image(GLenum_ target, GLint_ level, GLint_ internal, GLsizei_ w, GLsizei_ h, GLint_ border, GLenum_ fmt, GLenum_ type, const void *data) {
    (void)target; (void)level; (void)internal; (void)w; (void)h; (void)border; (void)fmt; (void)type; (void)data;
}
static void CUI_GLAPI tex_subimage(GLenum_ target, GLint_ level, GLint_ x, GLint_ y, GLsizei_ w, GLsizei_ h, GLenum_ fmt, GLenum_ type, const void *data) {
    (void)target; (void)level; (void)x; (void)w; (void)fmt; (void)type;
    if (uploads < 4) { upload_pixels[uploads] = data; upload_y[uploads] = y; upload_height[uploads] = h; }
    uploads++;
}

int main(void) {
    failed_shader = GL_FRAGMENT_SHADER_;
    CHECK(cui_gl_create(shader_loader) == NULL && shader_count == 0);
    failed_shader = GL_VERTEX_SHADER_;
    CHECK(cui_gl_create(shader_loader) == NULL && shader_count == 0);
    cui_gl_renderer r = {0};
    r.gl.GenTextures = gen_textures;
    r.gl.BindTexture = bind_texture;
    r.gl.TexParameteri = tex_parameter;
    r.gl.PixelStorei = pixel_store;
    r.gl.TexImage2D = tex_image;
    r.gl.TexSubImage2D = tex_subimage;
    for (int i = 0; i < 16; i++) CHECK(cui__gl_tex_create(&r, 4, 4, CUI_TEXTURE_RGBA8, NULL) != 0);
    fail_alloc = true;
    CHECK(cui__gl_tex_create(&r, 4, 4, CUI_TEXTURE_RGBA8, NULL) == 0);
    CHECK(r.tex_cap == 16 && r.tex_count == 16);
    fail_alloc = false;
    CHECK(cui__gl_tex_create(&r, 4, 4, CUI_TEXTURE_RGBA8, NULL) == 17);
    CHECK(r.tex_cap >= 17 && r.tex_count == 17);
    unsigned char pixels[68] = {0};
    cui__gl_tex_update(&r, 1, -1, 0, 4, 4, pixels, 16);
    cui__gl_tex_update(&r, 1, 0, 0, 5, 4, pixels, 20);
    cui__gl_tex_update(&r, 1, 0, 0, 4, 4, pixels, 1);
    CHECK(uploads == 0);
    cui__gl_tex_update(&r, 1, 0, 0, 4, 4, pixels, 17);
    CHECK(uploads == 4);
    for (int row = 0; row < 4; row++) {
        CHECK(upload_pixels[row] == pixels + row * 17);
        CHECK(upload_y[row] == row && upload_height[row] == 1);
    }
    uploads = 0;
    cui__gl_tex_update(&r, 1, 0, 0, 4, 4, pixels, 16);
    CHECK(uploads == 1 && upload_height[0] == 4);
    free(r.tex);
    printf("GL texture regressions: %d failed\n", failed);
    return failed != 0;
}
