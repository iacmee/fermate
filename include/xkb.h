// xkb_minilib.c (metti tutto in un .c singolo o separa in .h/.c)

#define _GNU_SOURCE
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

/* ==========================
   Sezione: tipi e API pubblica
   ========================== */

typedef struct {
    struct xkb_context *ctx;
    struct xkb_keymap  *keymap;
    struct xkb_state   *state;

    /* Compose (opzionale ma consigliato) */
    struct xkb_compose_table *compose_table;
    struct xkb_compose_state *compose_state;
} XkbEnv;

/* --- Setup keymap/state --- */

/* 1) Evdev (RMLVO): creare keymap da rules/model/layout/variant/options */
static MAYBE_UNUSED bool xkb_env_init_from_rmlvo(XkbEnv *env,
                                    const char *model,   /* es. "pc105"  */
                                    const char *layout,  /* es. "it"     */
                                    const char *variant, /* es. NULL     */
                                    const char *options) /* es. NULL     */
{
    memset(env, 0, sizeof(*env));

    env->ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!env->ctx) { fprintf(stderr, "xkb_context_new failed\n"); return false; }

    struct xkb_rule_names names = {
        .rules  = NULL,      /* usa default di sistema (regola "evdev") */
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

/* 2) Wayland: creare keymap da stringa ricevuta dal compositor */
static MAYBE_UNUSED bool xkb_env_init_from_keymap_string(XkbEnv *env,
                                            const char *keymap_string,
                                            enum xkb_keymap_format fmt /* tipicamente XKB_KEYMAP_FORMAT_TEXT_V1 */)
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

/* 3) X11: usa xkbcommon-x11 per ottenere keymap/state già pronti
   (qui lasciamo l'init a carico del chiamante; basta assegnare keymap+state dentro env)
*/

/* --- Abilitare Compose (dead keys, sequenze) --- */
static MAYBE_UNUSED bool xkb_env_enable_compose(XkbEnv *env, const char *locale /* es. "it_IT.UTF-8" o NULL per usare env */)
{
    if (!locale) {
        /* usa il locale di processo; assicurati altrove di aver chiamato setlocale(LC_CTYPE,"") */
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

/* --- Aggiornare lo stato in base al backend --- */

/* Evdev: passagli il code "evdev" (KEY_*), e il value (0=release,1=press,2=repeat).
   Ritorna il keycode nello spazio XKB (evdev+8). */
static MAYBE_UNUSED xkb_keycode_t xkb_update_from_evdev(XkbEnv *env, uint16_t ev_code, int ev_value)
{
    xkb_keycode_t kc = (xkb_keycode_t)(ev_code + 8); /* convenzione storica: +8 */
    if (ev_value == 1 || ev_value == 2) { /* press/repeat */
        xkb_state_update_key(env->state, kc, XKB_KEY_DOWN);
    } else if (ev_value == 0) {           /* release */
        xkb_state_update_key(env->state, kc, XKB_KEY_UP);
    }
    return kc;
}

/* X11/Wayland: applica la "mask" fornita dal server (depressed/latched/locked + group) */
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

/* --- Conversione EV -> UTF-8 (con compose opzionale)
   value: 1=press, 2=repeat, 0=release
   Ritorna:
     - true  se ha prodotto UTF-8 in out (len>0)
     - false se non c'è testo (tasto non stampabile o release)
   Se outsym != NULL e NON c'è testo, imposta outsym col keysym (es. frecce, ESC, F1...)
*/
static MAYBE_UNUSED bool xkb_ev_to_utf8(XkbEnv *env,
                           xkb_keycode_t kc,
                           int value,
                           char *out, size_t outsz,
                           xkb_keysym_t *outsym)
{
    if (outsym) *outsym = XKB_KEY_NoSymbol;
    if (!out || outsz == 0) return false;

    if (value == 0) {
        /* release: niente testo */
        out[0] = '\0';
        return false;
    }

    /* Se è un repeat ma la keymap dice "non ripetere", scarta */
    if (value == 2 && !xkb_keymap_key_repeats(env->keymap, kc)) {
        out[0] = '\0';
        return false;
    }

    /* Ottieni il keysym "logico" in base a keymap+state correnti */
    xkb_keysym_t sym = xkb_state_key_get_one_sym(env->state, kc);

    /* Se Compose è attivo, prova la composizione */
    if (env->compose_state) {
        xkb_compose_state_feed(env->compose_state, sym);
        switch (xkb_compose_state_get_status(env->compose_state)) {
            case XKB_COMPOSE_COMPOSED: {
                int n = xkb_compose_state_get_utf8(env->compose_state, out, outsz);
                xkb_compose_state_reset(env->compose_state);
                return n > 0;
            }
            case XKB_COMPOSE_COMPOSING:
                /* in attesa del prossimo tasto per completare la sequenza */
                out[0] = '\0';
                return false;
            case XKB_COMPOSE_CANCELLED:
                xkb_compose_state_reset(env->compose_state);
                /* ricadi alla risoluzione "semplice" */
                break;
            case XKB_COMPOSE_NOTHING:
                /* nessuna compose: ricadi alla risoluzione "semplice" */
                break;
        }
    }

    /* Risoluzione "semplice": prova a ottenere direttamente UTF-8 */
    int n = xkb_state_key_get_utf8(env->state, kc, out, outsz);
    if (n > 0) return true;

    /* Non c'è testo: restituisci almeno il keysym al chiamante */
    if (outsym) *outsym = sym;
    out[0] = '\0';
    return false;
}

/* --- Cleanup --- */
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

/* ==========================
   Fine mini-libreria
   ========================== */
