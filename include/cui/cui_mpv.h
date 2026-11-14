/* Optional libmpv playback. Enable CUI_VIDEO and link cui::video. */
#ifndef CUI_MPV_H
#define CUI_MPV_H
#include "cui_app.h"
#ifndef CUI_MPV_API
#define CUI_MPV_API
#endif
#ifdef __cplusplus
extern "C" {
#endif

typedef struct cui_mpv cui_mpv;

/* UI-thread API. app (NULL = current app) and its renderer must outlive the
 * player. Destroy before cui_app_destroy, with the same GL context current.
 * Uses hardware decoding where supported, otherwise CPU decoding. GL output
 * goes directly into a CUI texture; software output uses a reusable RGBA buffer.
 * Returns NULL on failure and optionally writes a negative libmpv error code.
 * User mpv configs, scripts, key bindings and on-screen controls are disabled. */
CUI_MPV_API cui_mpv *cui_mpv_create(cui_app *app, int *error);
CUI_MPV_API void cui_mpv_destroy(cui_mpv *player);
/* Asynchronous load/replace and start playback. path is copied by libmpv; it
 * may be a UTF-8 path or URL. Returns 0 if queued, negative on failure.
 * Actual load failures arrive through cui_mpv_last_error after update. */
CUI_MPV_API int cui_mpv_load(cui_mpv *player, const char *path);
/* Drain events and render only a new frame or resized target. Call each UI
 * frame, even when hidden. width/height are physical pixels, not points;
 * either <= 0 releases the output texture while audio/playback continues.
 * Size to the displayed surface, not the source video. No continuous app loop
 * is needed: libmpv wakes the app on events/frames, including while loading.
 * Returns true when the texture changed. State updates may occur either way. */
CUI_MPV_API bool cui_mpv_update(cui_mpv *player, int width, int height);
/* Snapshot for cui_video_player; copy it before passing it to the component.
 * Borrowed until the next update/load/destroy. The player owns frame. */
CUI_MPV_API const cui_video *cui_mpv_state(const cui_mpv *player);
/* Asynchronously apply CUI_PLAYER_TOGGLE/SEEK/VOLUME/MUTE from the component's
 * edited copy. Fullscreen remains the host window's responsibility. */
CUI_MPV_API int cui_mpv_apply(cui_mpv *player, const cui_video *edited, int events);
/* Last asynchronous/renderer error (0 = none), cleared by the next load.
 * The message is static libmpv text; never free it. NULL player is invalid. */
CUI_MPV_API int cui_mpv_last_error(const cui_mpv *player);
CUI_MPV_API const char *cui_mpv_error_string(int error);

#ifdef __cplusplus
}
#endif
#endif
