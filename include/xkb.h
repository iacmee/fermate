// XKB helpers for keymaps, keyboard state, and compose sequences.

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <locale.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-compose.h>


#if defined(__GNUC__) || defined(__clang__)
#  define MAYBE_UNUSED __attribute__((unused))
#else
#  define MAYBE_UNUSED
#endif

/* Public types and API */

typedef struct {
    struct xkb_context *ctx;
    struct xkb_keymap  *keymap;
    struct xkb_state   *state;

    /* Compose support (optional but recommended) */
    struct xkb_compose_table *compose_table;
    struct xkb_compose_state *compose_state;
} XkbEnv;

/* Keymap/state setup */

/* Evdev (RMLVO): create a keymap from rules/model/layout/variant/options. */
static MAYBE_UNUSED bool xkb_env_init_from_rmlvo(XkbEnv *env,
                                    const char *model,   /* e.g. "pc105" */
                                    const char *layout,  /* e.g. "it" */
                                    const char *variant, /* e.g. NULL */
                                    const char *options) /* e.g. NULL */
{
    memset(env, 0, sizeof(*env));

    env->ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!env->ctx) { fprintf(stderr, "xkb_context_new failed\n"); return false; }

    struct xkb_rule_names names = {
        .rules  = NULL,      /* Use the system default rules ("evdev"). */
        .model  = model,
        .layout = layout,
        .variant= variant,
        .options= options
    };

    env->keymap = xkb_keymap_new_from_names(env->ctx, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!env->keymap) { fprintf(stderr, "xkb_keymap_new_from_names failed\n"); return false; }

    env->state = xkb_state_new(env->keymap);
    if (!env->state) { fprintf(stderr, "xkb_state_new failed\n"); return false; }

    return true;
}

/* Wayland: create a keymap from a string received from the compositor. */
static MAYBE_UNUSED bool xkb_env_init_from_keymap_string(XkbEnv *env,
                                            const char *keymap_string,
                                            enum xkb_keymap_format fmt /* Usually XKB_KEYMAP_FORMAT_TEXT_V1 */)
{
    memset(env, 0, sizeof(*env));

    env->ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!env->ctx) { fprintf(stderr, "xkb_context_new failed\n"); return false; }

    env->keymap = xkb_keymap_new_from_string(env->ctx, keymap_string, fmt, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!env->keymap) { fprintf(stderr, "xkb_keymap_new_from_string failed\n"); return false; }

    env->state = xkb_state_new(env->keymap);
    if (!env->state) { fprintf(stderr, "xkb_state_new failed\n"); return false; }

    return true;
}

/* X11: use xkbcommon-x11 to obtain a keymap and state.
   The caller initializes them and assigns them to env. */

/* Enable Compose (dead keys and sequences). */
static MAYBE_UNUSED bool xkb_env_enable_compose(XkbEnv *env, const char *locale /* e.g. "it_IT.UTF-8", or NULL for the process locale */)
{
    if (!locale) {
        /* Use the process locale; the caller must first call setlocale(LC_CTYPE, ""). */
        locale = setlocale(LC_CTYPE, NULL);
        if (!locale) locale = "C";
    }

    env->compose_table = xkb_compose_table_new_from_locale(env->ctx, locale,
                                XKB_COMPOSE_COMPILE_NO_FLAGS);
    if (!env->compose_table) {
        fprintf(stderr, "xkb_compose_table_new_from_locale failed (locale=%s)\n", locale);
        return false;
    }

    env->compose_state = xkb_compose_state_new(env->compose_table,
                                XKB_COMPOSE_STATE_NO_FLAGS);
    if (!env->compose_state) {
        fprintf(stderr, "xkb_compose_state_new failed\n");
        return false;
    }
    return true;
}

/* Update state for the selected backend. */

/* Evdev: pass an evdev code (KEY_*) and value (0=release, 1=press, 2=repeat).
   Returns the XKB keycode (evdev+8). */
static MAYBE_UNUSED xkb_keycode_t xkb_update_from_evdev(XkbEnv *env, uint16_t ev_code, int ev_value)
{
    xkb_keycode_t kc = (xkb_keycode_t)(ev_code + 8); /* Historical offset: +8 */
    if (ev_value == 1 || ev_value == 2) { /* press/repeat */
        xkb_state_update_key(env->state, kc, XKB_KEY_DOWN);
    } else if (ev_value == 0) {           /* release */
        xkb_state_update_key(env->state, kc, XKB_KEY_UP);
    }
    return kc;
}

/* X11/Wayland: apply the server-provided mask (depressed/latched/locked + group). */
static MAYBE_UNUSED void xkb_update_from_mask(XkbEnv *env,
                                 xkb_mod_mask_t depressed_mods,
                                 xkb_mod_mask_t latched_mods,
                                 xkb_mod_mask_t locked_mods,
                                 xkb_layout_index_t depressed_group,
                                 xkb_layout_index_t latched_group,
                                 xkb_layout_index_t locked_group)
{
    xkb_state_update_mask(env->state,
                          depressed_mods, latched_mods, locked_mods,
                          depressed_group, latched_group, locked_group);
}

/* Convert an event to UTF-8, with optional Compose support.
   value: 1=press, 2=repeat, 0=release.
   Returns true when out contains UTF-8 text, false otherwise.
   If outsym is non-NULL and no text is produced, returns the keysym there
   (e.g. arrow keys, Escape, F1). */
static MAYBE_UNUSED bool xkb_ev_to_utf8(XkbEnv *env,
                           xkb_keycode_t kc,
                           int value,
                           char *out, size_t outsz,
                           xkb_keysym_t *outsym)
{
    if (outsym) *outsym = XKB_KEY_NoSymbol;
    if (!out || outsz == 0) return false;

    if (value == 0) {
        /* Release: no text. */
        out[0] = '\0';
        return false;
    }

    /* Ignore repeats when the keymap disables them. */
    if (value == 2 && !xkb_keymap_key_repeats(env->keymap, kc)) {
        out[0] = '\0';
        return false;
    }

    /* Get the logical keysym using the current keymap and state. */
    xkb_keysym_t sym = xkb_state_key_get_one_sym(env->state, kc);

    /* Try composition when Compose is enabled. */
    if (env->compose_state) {
        xkb_compose_state_feed(env->compose_state, sym);
        switch (xkb_compose_state_get_status(env->compose_state)) {
            case XKB_COMPOSE_COMPOSED: {
                int n = xkb_compose_state_get_utf8(env->compose_state, out, outsz);
                xkb_compose_state_reset(env->compose_state);
                return n > 0 && (size_t)n < outsz;
            }
            case XKB_COMPOSE_COMPOSING:
                /* Wait for the next key to complete the sequence. */
                out[0] = '\0';
                return false;
            case XKB_COMPOSE_CANCELLED:
                xkb_compose_state_reset(env->compose_state);
                /* Fall back to direct conversion. */
                break;
            case XKB_COMPOSE_NOTHING:
                /* No compose sequence: fall back to direct conversion. */
                break;
        }
    }

    /* Try direct UTF-8 conversion. */
    int n = xkb_state_key_get_utf8(env->state, kc, out, outsz);
    if (n > 0 && (size_t)n < outsz) return true;

    /* No text: return the keysym to the caller. */
    if (outsym) *outsym = sym;
    out[0] = '\0';
    return false;
}

/* Cleanup */
static MAYBE_UNUSED void xkb_env_destroy(XkbEnv *env)
{
    if (!env) return;
    if (env->compose_state)  xkb_compose_state_unref(env->compose_state);
    if (env->compose_table)  xkb_compose_table_unref(env->compose_table);
    if (env->state)          xkb_state_unref(env->state);
    if (env->keymap)         xkb_keymap_unref(env->keymap);
    if (env->ctx)            xkb_context_unref(env->ctx);
    memset(env, 0, sizeof(*env));
}

/* End of XKB helpers */
