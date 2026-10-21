#ifndef WIN_THEME_H
#define WIN_THEME_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>

typedef struct {
    COLORREF normal;
    COLORREF bright;
    COLORREF disabled;
    COLORREF muted;
    COLORREF dim;
    COLORREF bg_dark;
    COLORREF bg_box;
    COLORREF hover_bg;
    COLORREF border;
} win_theme_colors_t;

win_theme_colors_t win_get_current_theme_colors(void);

#define PLATO_PLASMA_COLOR     (win_get_current_theme_colors().normal)
#define PLATO_PLASMA_BRIGHT    (win_get_current_theme_colors().bright)
#define PLATO_PLASMA_DISABLED  (win_get_current_theme_colors().disabled)
#define PLATO_PLASMA_MUTED     (win_get_current_theme_colors().muted)
#define PLATO_PLASMA_DIM       (win_get_current_theme_colors().dim)
#define PLATO_BG_DARK          (win_get_current_theme_colors().bg_dark)
#define PLATO_BG_BOX           (win_get_current_theme_colors().bg_box)
#define PLATO_BG_HOVER         (win_get_current_theme_colors().hover_bg)

#endif /* WIN_THEME_H */
