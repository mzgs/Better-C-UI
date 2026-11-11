/* A busy operations console for comparing full builds with virtualized rows.
 * FPS includes rendering and display pacing; CPU is percent of one core. */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include <cui/cui.h>
#include <cui/cui_app.h>

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach/mach.h>
#endif
#endif

enum { STRESS_RECORDS = 100000, STRESS_HISTORY = 32, STRESS_POINTS = 256, STRESS_SERVICES = 8 };
static const int stress_counts[] = { 1000, 3000, 10000, 30000, STRESS_RECORDS };

typedef struct {
    double sample_at, cpu_at, fps, frame_ms, cpu_percent, rss_mib;
    unsigned frames;
    bool ready;
} stress_metrics;

typedef struct {
    char name[48];
    bool enabled, marked;
    float load, history[STRESS_HISTORY];
} stress_record;

typedef struct {
    stress_record records[STRESS_RECORDS];
    stress_metrics metrics;
    int workload, changes, selected, built_rows;
    bool virtualized, metrics_started;
    double elapsed, tick;
    float split, limits[STRESS_SERVICES];
    bool services[STRESS_SERVICES];
    float charts[4][3][STRESS_POINTS], heat[8 * 24];
} app_state;

static double stress_now(void) {
#ifdef _WIN32
    LARGE_INTEGER t, frequency;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart / (double)frequency.QuadPart;
#else
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t) != 0) return 0;
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
#endif
}

static double stress_cpu_seconds(void) {
#ifdef _WIN32
    FILETIME created, exited, kernel, user;
    ULARGE_INTEGER k, u;
    if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user)) return -1;
    k.LowPart = kernel.dwLowDateTime; k.HighPart = kernel.dwHighDateTime;
    u.LowPart = user.dwLowDateTime; u.HighPart = user.dwHighDateTime;
    return ((double)k.QuadPart + (double)u.QuadPart) * 1e-7;
#else
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) != 0) return -1;
    return usage.ru_utime.tv_sec + usage.ru_utime.tv_usec * 1e-6 +
           usage.ru_stime.tv_sec + usage.ru_stime.tv_usec * 1e-6;
#endif
}

static double stress_rss_mib(void) {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS counters;
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof counters)) return -1;
    return (double)counters.WorkingSetSize / (1024 * 1024);
#elif defined(__APPLE__)
    mach_task_basic_info_data_t info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &count) != KERN_SUCCESS) return -1;
    return (double)info.resident_size / (1024 * 1024);
#elif defined(__linux__)
    long total, resident, page_size = sysconf(_SC_PAGESIZE);
    FILE *file = fopen("/proc/self/statm", "r");
    if (!file) return -1;
    int fields = fscanf(file, "%ld %ld", &total, &resident);
    fclose(file);
    if (fields != 2 || resident < 0 || page_size <= 0) return -1;
    return (double)resident * (double)page_size / (1024 * 1024);
#else
    return -1;
#endif
}

static void stress_sample(stress_metrics *m, double now, double cpu, double rss) {
    double elapsed = now - m->sample_at;
    if (elapsed <= 0 || m->frames == 0) return;
    m->fps = m->frames / elapsed;
    m->frame_ms = elapsed * 1000 / m->frames;
    m->cpu_percent = cpu >= 0 && m->cpu_at >= 0 ? (cpu - m->cpu_at) * 100 / elapsed : -1;
    m->rss_mib = rss;
    m->sample_at = now; m->cpu_at = cpu; m->frames = 0; m->ready = true;
}

static void stress_monitor(app_state *a) {
    double now = stress_now();
    if (!a->metrics_started) {
        a->metrics_started = true;
        a->metrics.sample_at = now;
        a->metrics.cpu_at = stress_cpu_seconds();
        a->metrics.rss_mib = stress_rss_mib();
    } else {
        a->metrics.frames++;
        if (now - a->metrics.sample_at >= 0.5)
            stress_sample(&a->metrics, now, stress_cpu_seconds(), stress_rss_mib());
    }
}

static void stress_init(app_state *a) {
    memset(a, 0, sizeof *a);
    a->workload = 1;
    a->split = 0.73f;
    a->metrics.cpu_percent = a->metrics.rss_mib = -1;
    static const char *names[] = { "gateway", "search", "checkout", "catalog", "worker", "storage", "identity", "events" };
    for (int i = 0; i < STRESS_RECORDS; i++) {
        stress_record *r = &a->records[i];
        snprintf(r->name, sizeof r->name, "%s-eu-%05d", names[i % 8], i + 1);
        r->enabled = true;
        r->load = 20 + (float)(i % 67);
        for (int j = 0; j < STRESS_HISTORY; j++) r->history[j] = 40 + 25 * sinf(i * 0.17f + j * 0.3f);
    }
    for (int p = 0; p < 4; p++)
        for (int s = 0; s < 3; s++)
            for (int j = 0; j < STRESS_POINTS; j++)
                a->charts[p][s][j] = 45 + 18 * sinf(j * 0.07f + p + s) + 8 * cosf(j * 0.19f + s);
    for (int i = 0; i < 8 * 24; i++) a->heat[i] = (float)((i * 37 + i / 24 * 11) % 100);
    for (int i = 0; i < STRESS_SERVICES; i++) { a->services[i] = true; a->limits[i] = 60 + (float)i * 3; }
}

static void stress_advance(app_state *a, float dt) {
    if (a->changes == 2) return;
    a->elapsed += dt;
    a->tick += dt;
    if (a->tick < 0.1) return;
    a->tick = fmod(a->tick, 0.1);
    int begin = a->changes == 0 ? a->selected : 0;
    int end = a->changes == 0 ? begin + 1 : stress_counts[a->workload];
    for (int i = begin; i < end; i++) {
        stress_record *r = &a->records[i];
        r->load = 45 + 35 * sinf((float)a->elapsed * 1.6f + i * 0.03f);
        memmove(r->history, r->history + 1, (STRESS_HISTORY - 1) * sizeof(float));
        r->history[STRESS_HISTORY - 1] = r->load;
    }
    if (a->changes != 1) return;
    for (int p = 0; p < 4; p++) {
        for (int s = 0; s < 3; s++) {
            float *values = a->charts[p][s];
            memmove(values, values + 1, (STRESS_POINTS - 1) * sizeof(float));
            values[STRESS_POINTS - 1] = 45 + 25 * sinf((float)a->elapsed + p + s * 1.7f);
        }
    }
    for (int i = 0; i < 8 * 24; i++) a->heat[i] = 50 + 45 * sinf((float)a->elapsed + i * 0.31f);
}

static void stress_metric(cui_ctx *ui, const char *label, const char *value) {
    cui_column(ui, .gap = 2, .min_width = 78) {
        cui_text(ui, label, .size = 11, .color = CUI_COLOR_TEXT_SECONDARY);
        cui_text(ui, value, .font = CUI_FONT_MONO, .size = 17, .tabular = true, .nowrap = true);
    }
}

static void stress_header(cui_ctx *ui, app_state *a) {
    stress_metrics *m = &a->metrics;
    char fps[32] = "...", duration[32] = "...", cpu[32] = "...", rss[32] = "N/A";
    if (m->ready) {
        snprintf(fps, sizeof fps, "%.1f", m->fps);
        snprintf(duration, sizeof duration, "%.1f ms", m->frame_ms);
        if (m->cpu_percent >= 0) snprintf(cpu, sizeof cpu, "%.1f%%", m->cpu_percent);
        else snprintf(cpu, sizeof cpu, "N/A");
    }
    if (m->rss_mib >= 0) snprintf(rss, sizeof rss, "%.1f MiB", m->rss_mib);
    cui_row(ui, .width = CUI_FILL, .gap = 20, .pad_x = 20, .pad_y = 14, .wrap = true) {
        cui_column(ui, .width = CUI_FILL, .min_width = 180, .gap = 2) {
            cui_heading(ui, "Fleet control room", 3);
            cui_secondary(ui, "Immediate-mode stress test / simulated service data");
        }
        cui_row(ui, .gap = 20) {
            stress_metric(ui, "FPS", fps);
            stress_metric(ui, "Frame interval", duration);
            stress_metric(ui, "CPU / one core", cpu);
            stress_metric(ui, "Process RSS", rss);
        }
    }
}

static void stress_toolbar(cui_ctx *ui, app_state *a) {
    static const char *loads[] = { "1,000", "3,000", "10,000", "30,000", "100,000" };
    static const char *changes[] = { "One record", "All records", "Paused" };
    cui_row(ui, .width = CUI_FILL, .gap = 16, .pad_x = 20, .pad_y = 10, .wrap = true) {
        cui_label(ui, "Records");
        if (cui_segmented(ui, "workload", loads, 5, &a->workload, .size = CUI_SIZE_SM)) {
            if (a->selected >= stress_counts[a->workload]) a->selected = 0;
            cui_scroll_to(ui, "records", 0);
            cui_scroll_to(ui, "virtual-records", 0);
        }
        cui_segmented(ui, "changes", changes, 3, &a->changes, .size = CUI_SIZE_SM);
        cui_switch(ui, "Virtualize rows", &a->virtualized);
        cui_spacer(ui);
        if (cui_icon_button(ui, cui_get_theme_mode(ui) == CUI_THEME_DARK ? CUI_ICON_SUN : CUI_ICON_MOON, .tooltip = "Toggle theme"))
            cui_set_theme(ui, cui_get_theme_mode(ui) == CUI_THEME_DARK ? CUI_THEME_LIGHT : CUI_THEME_DARK);
    }
}

static void stress_charts(cui_ctx *ui, app_state *a) {
    static const char *titles[] = { "Request duration / ms", "Queue depth / jobs", "Throughput / req per second", "Storage latency / ms" };
    cui_box(ui, .dir = CUI_GRID, .columns = cui_viewport_size(ui).x < 1050 ? 2 : 4,
            .width = CUI_FILL, .gap = 12, .pad_x = 20, .pad_y = 12) {
        for (int p = 0; p < 4; p++) {
            cui_push_id_int(ui, p);
            cui_card(ui, .pad = 12, .gap = 8, .width = CUI_FILL) {
                cui_label(ui, titles[p]);
                cui_series series[3] = {
                    { "EU west", a->charts[p][0], STRESS_POINTS, { 0 } },
                    { "EU north", a->charts[p][1], STRESS_POINTS, { 0 } },
                    { "US east", a->charts[p][2], STRESS_POINTS, { 0 } }
                };
                cui_line_chart(ui, "trend", series, 3, .height = 132, .min = 0, .max = 100, .no_legend = true);
            }
            cui_pop_id(ui);
        }
    }
}

static void stress_record_row(cui_ctx *ui, app_state *a, int index) {
    stress_record *r = &a->records[index];
    bool wide = cui_viewport_size(ui).x >= 1200;
    cui_push_id_int(ui, index);
    cui_row(ui, .width = CUI_FILL, .height = CUI_PX(48), .gap = 12, .pad_x = 12,
            .bg = a->selected == index ? CUI_COLOR_SURFACE_MUTED : CUI_TRANSPARENT) {
        cui_checkbox(ui, "##marked", &r->marked);
        char label[32];
        snprintf(label, sizeof label, "%05d", index + 1);
        cui_text(ui, label, .font = CUI_FONT_MONO, .size = 12, .color = CUI_COLOR_TEXT_SECONDARY, .width = CUI_PX(48));
        cui_input(ui, "name", r->name, sizeof r->name, .size = CUI_SIZE_SM, .width = CUI_FILL);
        if (wide) {
            cui_badge(ui, r->load > 70 ? "Busy" : "Healthy", .tone = r->load > 70 ? CUI_TONE_WARNING : CUI_TONE_SUCCESS, .dot = true);
            cui_sparkline(ui, r->history, STRESS_HISTORY, CUI_PX(84), 24, CUI_COLOR_ACCENT);
        }
        cui_column(ui, .width = CUI_PX(82), .gap = 3) {
            snprintf(label, sizeof label, "%.1f%% load", (double)r->load);
            cui_text(ui, label, .size = 11, .tabular = true);
            cui_progress(ui, r->load / 100, .height = 3);
        }
        if (wide) cui_switch(ui, "Online", &r->enabled);
        if (cui_icon_button(ui, CUI_ICON_SETTINGS, .tooltip = "Inspect service")) a->selected = index;
    }
    cui_pop_id(ui);
    a->built_rows++;
}

static void stress_records(cui_ctx *ui, app_state *a) {
    int count = stress_counts[a->workload], first = 0, last = count;
    cui_column(ui, .width = CUI_FILL, .height = CUI_FILL) {
        cui_row(ui, .width = CUI_FILL, .pad = 12, .gap = 12) {
            cui_label(ui, "Service instances");
            cui_spacer(ui);
            cui_muted(ui, a->virtualized ? "Only visible rows are built" : "Every row is built, including offscreen rows");
        }
        cui_separator(ui);
        a->built_rows = 0;
        if (a->virtualized) {
            cui_virtual_list_begin(ui, "virtual-records", count, 48, CUI_FILL, &first, &last);
            for (int i = first; i < last; i++) stress_record_row(ui, a, i);
            cui_virtual_list_end(ui);
        } else {
            cui_scroll(ui, .id = "records", .width = CUI_FILL, .height = CUI_FILL) {
                for (int i = 0; i < count; i++) stress_record_row(ui, a, i);
            }
        }
    }
}

static void stress_inspector(cui_ctx *ui, app_state *a) {
    static const char *names[] = { "Gateway", "Search", "Checkout", "Catalog", "Workers", "Storage", "Identity", "Events" };
    cui_scroll(ui, .id = "inspector", .width = CUI_FILL, .height = CUI_FILL, .pad = 16, .gap = 12) {
        cui_label(ui, "Selected instance");
        cui_text(ui, a->records[a->selected].name, .font = CUI_FONT_MONO, .size = 13, .nowrap = true);
        cui_switch(ui, "Accept traffic", &a->records[a->selected].enabled);
        cui_separator(ui);
        cui_label(ui, "Traffic by zone / last 24 samples");
        cui_heatmap(ui, "traffic", a->heat, 8, 24, .min = 0, .max = 100, .width = CUI_FILL, .format = "%.0f%%");
        cui_separator(ui);
        cui_label(ui, "Service budgets");
        for (int i = 0; i < STRESS_SERVICES; i++) {
            cui_push_id_int(ui, i);
            cui_row(ui, .width = CUI_FILL) {
                cui_text(ui, names[i], .width = CUI_FILL);
                cui_switch(ui, "Route", &a->services[i]);
            }
            cui_slider(ui, "Budget", &a->limits[i], 0, 100, .show_value = true, .format = "%.0f%%");
            cui_pop_id(ui);
        }
        cui_separator(ui);
        cui_label(ui, "Deployment configuration");
        cui_code_block(ui, "config", "{\n  \"region\": \"eu-west\",\n  \"replicas\": 12,\n  \"timeout_ms\": 250,\n  \"retry_budget\": 0.05,\n  \"health_check\": \"/ready\"\n}",
                       .lang = "json", .line_numbers = true, .max_height = 180, .size = 11);
        cui_label(ui, "Recent deployment events");
        for (int i = 0; i < 32; i++) {
            char line[100];
            snprintf(line, sizeof line, "14:%02d:%02d  %s / health check passed", 58 - i, i * 7 % 60, names[i % 8]);
            cui_text(ui, line, .size = 11, .font = CUI_FONT_MONO, .nowrap = true, .color = CUI_COLOR_TEXT_SECONDARY);
        }
    }
}

static void frame(cui_ctx *ui, void *user) {
    app_state *a = user;
    stress_monitor(a);
    stress_advance(a, cui_delta_time(ui));
    cui_column(ui, .width = CUI_FILL, .height = CUI_FILL, .bg = CUI_COLOR_BACKGROUND) {
        stress_header(ui, a);
        cui_separator(ui);
        stress_toolbar(ui, a);
        cui_separator(ui);
        stress_charts(ui, a);
        cui_separator(ui);
        if (cui_viewport_size(ui).x >= 1000) {
            cui_split(ui, "workspace", &a->split, .min_first = 440, .min_second = 280, .height = CUI_FILL, .width = CUI_FILL) {
                stress_records(ui, a);
                cui_split_next(ui);
                stress_inspector(ui, a);
            }
        } else {
            stress_records(ui, a);
        }
        cui_separator(ui);
        cui_row(ui, .width = CUI_FILL, .pad_x = 20, .pad_y = 8, .gap = 12, .wrap = true) {
            char summary[100];
            const char *renderer = cui_app_ui(NULL) == ui ? (cui_app_is_software(NULL) ? "Software" : "OpenGL") : "Headless";
            snprintf(summary, sizeof summary, "%s / %d / %d rows built per frame", renderer, a->built_rows, stress_counts[a->workload]);
            cui_text(ui, summary, .size = 12, .font = CUI_FONT_MONO);
            cui_spacer(ui);
            cui_muted(ui, "Continuous redraw / display-paced FPS / metrics sampled every 0.5 s");
        }
    }
}

int main(void) {
    static app_state state;
    stress_init(&state);
    return cui_app_run(&(cui_app_desc){ .title = "Better-C-UI / Stress test", .width = 1600, .height = 1000,
                         .min_width = 640, .min_height = 720, .continuous = true,
                         .theme = CUI_THEME_DARK, .accent = CUI_ACCENT_GREEN }, frame, &state);
}
