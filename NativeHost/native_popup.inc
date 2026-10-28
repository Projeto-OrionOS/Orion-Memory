/* Lightweight Win32 popup menus. The tray's region submenu intentionally mimics
   the Mem Reduct interaction: compact rows, side submenu, immediate checkmarks.
   These changes do not affect the main or settings windows. */
#define POPUP_CLASS L"OrionMemory.RoundedPopup"
#define CMD_DARK 200
#define CMD_TOPMOST 201
#define CMD_REFRESH 202
#define CMD_GRAPH 203

typedef struct {
    UINT command;
    int separator, submenu, checked, disabled;
    wchar_t text[256];
} PopupRow;

typedef struct {
    HWND hwnd, owner, return_focus, return_foreground;
    int kind, count, selected, width, height;
    int *creation_disposed;
    HFONT font;
    HPEN mark_pen, disabled_pen, separator_pen;
    PopupRow rows[16];
} Popup;

static HWND g_popup_main, g_popup_sub;

static void popup_close_all(void) {
    HWND sub = g_popup_sub, main = g_popup_main;
    g_popup_sub = g_popup_main = NULL;
    if (sub) DestroyWindow(sub);
    if (main) DestroyWindow(main);
}

static int popup_portuguese(void) {
    return !_wcsicmp(g_settings.language, L"pt-BR") ||
           !_wcsicmp(g_settings.language, L"pt-PT");
}

static const wchar_t *const tray_region_short_labels[9] = {
    L"Conjunto de trabalho", L"Cache do sistema", L"Páginas modificadas *",
    L"Lista de espera completa *", L"Espera de baixa prioridade",
    L"Cache modificado", L"Cache do Registro", L"Combinação de páginas",
    L"Conjuntos em 2º plano"
};

static void popup_add(Popup *p, UINT command, const char *text,
                      int checked, int disabled, int submenu) {
    if (p->count >= 16) return;
    PopupRow *row = &p->rows[p->count++];
    memset(row, 0, sizeof(*row));
    row->command = command;
    row->checked = checked;
    row->disabled = disabled;
    row->submenu = submenu;
    row->separator = text == NULL;
    if (text) translate(text, row->text, 256);
}

static void popup_build(Popup *p) {
    p->count = 0;
    if (p->kind == 4) {
        int count = 0;
        for (int i = 0; i < 9; ++i) count += g_settings.regions[i];
        for (int i = 0; i < 9; ++i) {
            popup_add(p, CMD_REGION + i, region_labels[i], g_settings.regions[i],
                      g_settings.regions[i] && count <= 1, 0);
            /* Only abbreviate the tray submenu. Settings and cleanup logs keep
               their full, precise descriptions and all locales remain supported. */
            if (popup_portuguese())
                copyw(p->rows[p->count - 1].text, 256, tray_region_short_labels[i]);
        }
        return;
    }
    if (p->kind == 0) {
        popup_add(p, CMD_SHOW, "Abrir Orion Memory", 0, 0, 0);
        popup_add(p, CMD_CLEAN, "Limpar memória agora", 0,
                  g_busy || native_authorization_busy(), 0);
        popup_add(p, 0, NULL, 0, 1, 0);
        popup_add(p, 0, "Partes da memória para limpar", 0, 0, 1);
        if (popup_portuguese()) copyw(p->rows[p->count - 1].text, 256, L"Áreas de limpeza");
        popup_add(p, 0, NULL, 0, 1, 0);
        popup_add(p, CMD_SETTINGS, "Configurações", 0, 0, 0);
        popup_add(p, CMD_HISTORY, "Histórico de limpezas", 0, 0, 0);
        popup_add(p, CMD_ABOUT, "Sobre o Orion Memory", 0, 0, 0);
        popup_add(p, 0, NULL, 0, 1, 0);
        popup_add(p, CMD_EXIT, "Sair", 0, g_busy || native_authorization_busy(), 0);
    } else if (p->kind == 1) {
        popup_add(p, CMD_CLEAN, "Limpar memória agora", 0,
                  g_busy || native_authorization_busy(), 0);
        popup_add(p, CMD_HISTORY, "Histórico de limpezas", 0, 0, 0);
        popup_add(p, 0, NULL, 0, 1, 0);
        popup_add(p, CMD_EXIT, "Sair", 0,
                  g_busy || native_authorization_busy(), 0);
    } else if (p->kind == 2) {
        popup_add(p, CMD_TOPMOST, "Manter a janela principal sempre na frente",
                  g_settings.always_on_top, 0, 0);
        popup_add(p, CMD_DARK, "Usar tema escuro", g_settings.dark, 0, 0);
        popup_add(p, CMD_GRAPH, "Mostrar gráfico de memória na janela principal",
                  g_settings.show_graph, 0, 0);
        popup_add(p, 0, NULL, 0, 1, 0);
        popup_add(p, CMD_REFRESH, "Atualizar", 0, 0, 0);
    } else {
        popup_add(p, CMD_ABOUT, "Sobre o Orion Memory", 0, 0, 0);
    }
}

static int popup_row_height(Popup *p, int index) {
    return ui_scale(p->hwnd, p->rows[index].separator ? 8 :
                    (p->kind == 4 ? 26 : p->kind == 0 ? 28 : 32));
}

static int popup_row_top(Popup *p, int index) {
    int y = ui_scale(p->hwnd, 5);
    for (int i = 0; i < index; ++i) y += popup_row_height(p, i);
    return y;
}

static RECT popup_row_rect(Popup *p, int index) {
    int y = popup_row_top(p, index);
    int inset = ui_scale(p->hwnd, 4);
    RECT r = {inset, y, p->width - inset, y + popup_row_height(p, index)};
    return r;
}

static int popup_hit(Popup *p, int y) {
    for (int i = 0; i < p->count; ++i) {
        int top = popup_row_top(p, i);
        if (y >= top && y < top + popup_row_height(p, i))
            return p->rows[i].separator || p->rows[i].disabled ? -1 : i;
    }
    return -1;
}

static void popup_select(Popup *p, int index) {
    if (index == p->selected) return;
    int previous = p->selected;
    p->selected = index;
    if (previous >= 0) {
        RECT r = popup_row_rect(p, previous);
        InvalidateRect(p->hwnd, &r, FALSE);
    }
    if (index >= 0) {
        RECT r = popup_row_rect(p, index);
        InvalidateRect(p->hwnd, &r, FALSE);
    }
}

static void popup_paint_window(HWND hwnd, HDC dc) {
    Popup *p = (Popup *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (!p) return;
    RECT client, clip;
    GetClientRect(hwnd, &client);
    int clipping = GetClipBox(dc, &clip);
    if (clipping == ERROR || clipping == NULLREGION) return;
    ui_round_border(dc, client, ui_surface(), ui_outline(), ui_scale(hwnd, 8));
    for (int i = 0; i < p->count; ++i) {
        PopupRow *row = &p->rows[i];
        RECT area = popup_row_rect(p, i), overlap;
        if (!IntersectRect(&overlap, &area, &clip)) continue;
        int cy = (area.top + area.bottom) / 2;
        if (row->separator) {
            HGDIOBJ old = SelectObject(dc, p->separator_pen);
            MoveToEx(dc, ui_scale(hwnd, 12), cy, NULL);
            LineTo(dc, p->width - ui_scale(hwnd, 12), cy);
            SelectObject(dc, old);
            continue;
        }
        int selected = p->selected == i;
        if (selected) ui_round(dc, area, ui_selection_bg(),
                               ui_scale(hwnd, (p->kind == 0 || p->kind == 4) ? 5 : 6));
        COLORREF fg = row->disabled ? ui_muted() :
                      selected ? ui_selection_fg() : ui_text();
        RECT text = area;
        text.left += ui_scale(hwnd, 28);
        text.right -= ui_scale(hwnd, row->submenu ? 20 : 12);
        ui_text_draw(dc, text, row->text, p->font, fg, DT_LEFT | DT_VCENTER |
                     DT_SINGLELINE | DT_END_ELLIPSIS);
        HGDIOBJ old = SelectObject(dc, row->disabled ? p->disabled_pen : p->mark_pen);
        if (row->checked) {
            int cx = ui_scale(hwnd, 17);
            MoveToEx(dc, cx - ui_scale(hwnd, 4), cy, NULL);
            LineTo(dc, cx - ui_scale(hwnd, 1), cy + ui_scale(hwnd, 3));
            LineTo(dc, cx + ui_scale(hwnd, 5), cy - ui_scale(hwnd, 4));
        }
        if (row->submenu) {
            SelectObject(dc, p->disabled_pen);
            int cx = p->width - ui_scale(hwnd, 13);
            MoveToEx(dc, cx - ui_scale(hwnd, 2), cy - ui_scale(hwnd, 4), NULL);
            LineTo(dc, cx + ui_scale(hwnd, 2), cy);
            LineTo(dc, cx - ui_scale(hwnd, 2), cy + ui_scale(hwnd, 4));
        }
        SelectObject(dc, old);
    }
}

static void native_execute_command(UINT command);
static HWND popup_create(HWND owner, int kind, POINT point, int bottom_align, int left_align);

static void popup_open_sub(Popup *parent, int focus) {
    if (parent->selected < 0 || !parent->rows[parent->selected].submenu) return;
    if (!g_popup_sub) {
        RECT r;
        GetWindowRect(parent->hwnd, &r);
        int gap = ui_scale(parent->hwnd, 3);
        POINT point = {r.right + gap,
                       r.top + popup_row_top(parent, parent->selected) - ui_scale(parent->hwnd, 5)};
        MONITORINFO monitor;
        memset(&monitor, 0, sizeof(monitor));
        monitor.cbSize = sizeof(monitor);
        int open_left = 0;
        if (GetMonitorInfoW(MonitorFromWindow(parent->hwnd, MONITOR_DEFAULTTONEAREST), &monitor)) {
            /* The Windows tray is normally near the right edge. Prefer opening
               to the left instead of letting the OS clamp over the parent. */
            int room_right = monitor.rcWork.right - r.right;
            int room_left = r.left - monitor.rcWork.left;
            int estimate = ui_scale(parent->hwnd, popup_portuguese() ? 225 : 330);
            open_left = room_right < estimate && room_left > room_right;
        }
        if (open_left) point.x = r.left - gap;
        g_popup_sub = popup_create(parent->owner, 4, point, 0, open_left);
    }
    if (focus && g_popup_sub) {
        SetFocus(g_popup_sub);
        Popup *sub = (Popup *)GetWindowLongPtrW(g_popup_sub, GWLP_USERDATA);
        if (sub) {
            for (int i = 0; i < sub->count; ++i) {
                if (!sub->rows[i].disabled && !sub->rows[i].separator) {
                    popup_select(sub, i);
                    break;
                }
            }
        }
    }
}

static void popup_cancel(Popup *p) {
    HWND foreground = p->return_foreground, focus = p->return_focus;
    popup_close_all();
    if (foreground && IsWindow(foreground)) SetForegroundWindow(foreground);
    if (focus && IsWindow(focus) &&
        GetWindowThreadProcessId(focus, NULL) == GetCurrentThreadId()) SetFocus(focus);
}

static void popup_close_sub(void) {
    HWND sub = g_popup_sub;
    g_popup_sub = NULL;
    if (g_popup_main) SetFocus(g_popup_main);
    if (sub) DestroyWindow(sub);
}

static void popup_activate_row(Popup *p, int index) {
    if (index < 0 || index >= p->count ||
        p->rows[index].disabled || p->rows[index].separator) return;
    popup_select(p, index);
    if (p->rows[index].submenu) {
        popup_open_sub(p, 1);
        return;
    }
    UINT command = p->rows[index].command;
    if (p->kind == 4 && command >= CMD_REGION && command < CMD_REGION + 9) {
        /* Mem Reduct behavior: save on the check click, with no Apply dialog.
           Keep the side menu open for selecting several regions efficiently. */
        HWND popup_hwnd = p->hwnd;
        native_execute_command(command);
        if (!IsWindow(popup_hwnd)) return;
        p = (Popup *)GetWindowLongPtrW(popup_hwnd, GWLP_USERDATA);
        if (!p) return;
        popup_build(p);
        if (p->rows[index].disabled) popup_select(p, -1);
        InvalidateRect(popup_hwnd, NULL, FALSE);
        return;
    }
    popup_close_all();
    native_execute_command(command);
}

static LRESULT CALLBACK popup_proc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
    Popup *p = (Popup *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (message == WM_NCCREATE) {
        p = (Popup *)((CREATESTRUCTW *)lp)->lpCreateParams;
        p->hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)p);
    }
    if (!p) return DefWindowProcW(hwnd, message, wp, lp);
    switch (message) {
        case WM_GETDLGCODE:
            return DLGC_WANTARROWS | DLGC_WANTALLKEYS;
        case WM_MOUSEMOVE: {
            int row = popup_hit(p, GET_Y_LPARAM(lp));
            if (row != p->selected) {
                popup_select(p, row);
                if (row >= 0 && p->rows[row].submenu) popup_open_sub(p, 0);
                else if (p->kind != 4 && g_popup_sub) popup_close_sub();
            }
            return 0;
        }
        case WM_LBUTTONUP:
        case WM_RBUTTONUP: {
            int row = popup_hit(p, GET_Y_LPARAM(lp));
            popup_activate_row(p, row);
            return 0;
        }
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) {
                if (g_popup_sub == hwnd) popup_close_sub();
                else popup_cancel(p);
                return 0;
            }
            if (wp == VK_LEFT && p->kind == 4) {
                popup_close_sub();
                return 0;
            }
            if (wp == VK_RIGHT) {
                popup_open_sub(p, 1);
                return 0;
            }
            if (wp == VK_RETURN || wp == VK_SPACE) {
                popup_activate_row(p, p->selected);
                return 0;
            }
            if (wp == VK_DOWN || wp == VK_UP || wp == VK_HOME || wp == VK_END) {
                int direction = (wp == VK_UP || wp == VK_END) ? -1 : 1;
                int current = (wp == VK_HOME) ? -1 :
                              (wp == VK_END) ? 0 : p->selected;
                if (current < 0 && direction < 0) current = 0;
                for (int tries = 0; tries < p->count; ++tries) {
                    current = (current + direction + p->count) % p->count;
                    if (!p->rows[current].separator && !p->rows[current].disabled) {
                        popup_select(p, current);
                        if (p->kind != 4 && g_popup_sub && !p->rows[current].submenu)
                            popup_close_sub();
                        break;
                    }
                }
                return 0;
            }
            return 0;
        case WM_ACTIVATE:
            if (LOWORD(wp) == WA_INACTIVE) {
                HWND next = (HWND)lp;
                if (next != g_popup_main && next != g_popup_sub) popup_close_all();
            }
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT paint;
            HDC dc = BeginPaint(hwnd, &paint);
            ui_paint_buffered(hwnd, dc, &paint.rcPaint, popup_paint_window);
            EndPaint(hwnd, &paint);
            return 0;
        }
        case WM_PRINTCLIENT:
            popup_paint_window(hwnd, (HDC)wp);
            return 0;
        case WM_NCDESTROY:
            if (g_popup_main == hwnd) g_popup_main = NULL;
            if (g_popup_sub == hwnd) g_popup_sub = NULL;
            if (p->creation_disposed) *p->creation_disposed = 1;
            if (p->font) DeleteObject(p->font);
            if (p->mark_pen) DeleteObject(p->mark_pen);
            if (p->disabled_pen) DeleteObject(p->disabled_pen);
            if (p->separator_pen) DeleteObject(p->separator_pen);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            free(p);
            return DefWindowProcW(hwnd, message, wp, lp);
    }
    return DefWindowProcW(hwnd, message, wp, lp);
}

static HWND popup_create(HWND owner, int kind, POINT point, int bottom_align, int left_align) {
    WNDCLASSEXW cls;
    memset(&cls, 0, sizeof(cls));
    cls.cbSize = sizeof(cls);
    cls.lpfnWndProc = popup_proc;
    cls.hInstance = g_instance;
    cls.hCursor = LoadCursorW(NULL, IDC_ARROW);
    cls.style = CS_DROPSHADOW;
    cls.lpszClassName = POPUP_CLASS;
    RegisterClassExW(&cls);

    Popup *p = (Popup *)calloc(1, sizeof(*p));
    if (!p) return NULL;
    p->owner = owner;
    p->kind = kind;
    p->selected = -1;
    p->return_focus = GetFocus();
    p->return_foreground = GetForegroundWindow();
    popup_build(p);
    int disposed = 0;
    p->creation_disposed = &disposed;
    HWND hwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, POPUP_CLASS, L"",
                                WS_POPUP, point.x, point.y, 10, 10,
                                owner, NULL, g_instance, p);
    if (!hwnd) {
        if (!disposed) free(p);
        return NULL;
    }
    p->creation_disposed = NULL;
    p->font = ui_font(hwnd, kind == 4 || kind == 0 ? 11 : 12, FW_NORMAL);
    p->mark_pen = CreatePen(PS_SOLID, max(1, ui_scale(hwnd, 1)), ui_accent());
    p->disabled_pen = CreatePen(PS_SOLID, max(1, ui_scale(hwnd, 1)), ui_muted());
    p->separator_pen = CreatePen(PS_SOLID, 1, ui_outline());

    HDC dc = GetDC(hwnd);
    HGDIOBJ old = SelectObject(dc, p->font);
    p->width = ui_scale(hwnd, kind == 4 ? 212 : kind == 0 ? 210 : 236);
    p->height = ui_scale(hwnd, 10);
    for (int i = 0; i < p->count; ++i) {
        SIZE size = {0};
        GetTextExtentPoint32W(dc, p->rows[i].text,
                              (int)wcslen(p->rows[i].text), &size);
        int required = size.cx + ui_scale(hwnd, p->rows[i].submenu ? 54 : 43);
        if (required > p->width) p->width = required;
        p->height += popup_row_height(p, i);
    }
    SelectObject(dc, old);
    ReleaseDC(hwnd, dc);

    MONITORINFO monitor;
    memset(&monitor, 0, sizeof(monitor));
    monitor.cbSize = sizeof(monitor);
    GetMonitorInfoW(MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST), &monitor);
    if (bottom_align) {
        point.x -= p->width;
        point.y -= p->height;
    } else if (left_align) {
        point.x -= p->width;
    } else if (kind == 4 && point.x + p->width > monitor.rcWork.right && g_popup_main) {
        /* Long translations may need more width than the initial estimate. */
        RECT parent_rect;
        GetWindowRect(g_popup_main, &parent_rect);
        point.x = parent_rect.left - ui_scale(hwnd, 3) - p->width;
    }
    if (point.x + p->width > monitor.rcWork.right) point.x = monitor.rcWork.right - p->width;
    if (point.y + p->height > monitor.rcWork.bottom) point.y = monitor.rcWork.bottom - p->height;
    if (point.x < monitor.rcWork.left) point.x = monitor.rcWork.left;
    if (point.y < monitor.rcWork.top) point.y = monitor.rcWork.top;

    SetWindowPos(hwnd, HWND_TOPMOST, point.x, point.y, p->width, p->height,
                 SWP_NOACTIVATE);
    HRGN region = CreateRoundRectRgn(0, 0, p->width + 1, p->height + 1,
                                      ui_scale(hwnd, (kind == 0 || kind == 4) ? 14 : 16),
                                      ui_scale(hwnd, (kind == 0 || kind == 4) ? 14 : 16));
    if (!SetWindowRgn(hwnd, region, FALSE)) DeleteObject(region);
    ui_apply_window(hwnd);
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    return hwnd;
}

static void native_popup_open(HWND owner, int kind, POINT point) {
    popup_close_all();
    g_popup_main = popup_create(owner, kind, point, kind == 0, 0);
    if (g_popup_main) {
        SetForegroundWindow(g_popup_main);
        SetFocus(g_popup_main);
    }
}
