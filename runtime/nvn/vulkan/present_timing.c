// What reaches the screen, and when (VK_KHR_present_id + VK_KHR_present_wait, where the driver has them): every
// present carries an id, and a thread of its own waits for each in turn (vkWaitForPresentKHR returns once the
// compositor has the image) and notes the time; the display shows it at its next vblank (plat_display_vblank). Every
// ~10 s of play:
//   [present] 300 frames shown in 10.0 s: 33.3 ms apart (33.3 to 33.3), 0 off the 33.3 ms cadence, 0 replaced;
//   display 60.00 Hz, frames held 1 / 2 / 3+ refreshes: 0 / 300 / 0, taken in each quarter of a refresh: 300 0 0 0
// "off the cadence": shown a refresh early or late against the one before - on a 60 Hz display at 30 fps a frame
// held for one refresh or three instead of two, the judder an average frame rate doesn't show. "replaced": a newer
// image took its place before it was shown (MAILBOX). The display pacing (runtime/nvn/objects.c) reads two counts:
// frames taken well after their slot (present_timing_lateness: its lead grows) and frames off the cadence while locked
// (present_timing_cadence: its safety net). LARECOMP_PRESENT_TIMING=0: off; =2: one line per frame.
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vk.h"
#include "../../platform.h"

static PFN_vkWaitForPresentKHR wait_present;
static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;     // (queued_pub, wait_sc: short holds)
static pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t sc_mu = PTHREAD_MUTEX_INITIALIZER;  // (held across a wait: the swapchain isn't destroyed then)
static uint64_t queued;              // (render thread) the last id given to a present
static uint64_t queued_at[256];      // (render thread) when each id's vkQueuePresentKHR returned (by id & 255)
static uint64_t target_at[256];      // (render thread) its slot (0: none)
static _Atomic uint64_t n_taken, n_late;  // frames taken; those taken a third of a refresh or more after their slot
static _Atomic uint64_t n_cad, n_cad_off;  // frames counted in refreshes; those off the cadence
static _Atomic uint64_t queued_pub;  // the same, for the waiter
static VkSwapchainKHR wait_sc;       // the swapchain those ids are on (mu)
static int on = -1, verbose;

bool present_timing_wanted(void) {
    if (on < 0) {
        const char *e = getenv("LARECOMP_PRESENT_TIMING");
        on = !e || atoi(e) != 0;
        verbose = e && atoi(e) >= 2;
    }
    return on;
}


// (expected frame period in ns: what the display pacing aims at; vi.c / objects.c)
extern volatile uint64_t g_vsync_period;
extern int g_present_interval;     // (objects.c: the game's present interval, 0 before its first present)
extern uint64_t g_pace_period_ns;  // (objects.c: the frame period present_pace keeps, locked to the display: 0 not yet)

// t: when the compositor took the frame (it's shown at the display's next vblank after that); counted in vblanks
// where the display's timing is known (plat_display_vblank), else in ms against the expected frame period
static void stats(uint64_t t, uint64_t id, uint64_t id_before) {
    static uint64_t prev, t0, n, off, replaced, min, max, by_vb[4], phase[4];
    static double per_ms;
    uint64_t want = g_pace_period_ns ? g_pace_period_ns
                                     : g_vsync_period * (uint64_t)(g_present_interval > 0 ? g_present_interval : 1);
    uint64_t vb, per, shown = t;
    if (plat_display_vblank(&vb, &per)) {
        if (vb > t) vb -= (vb - t + per - 1) / per * per;  // (the vblank at or before t)
        shown = vb + per;  // (the vblank it's shown at)
        phase[((t - vb) * 4 / per) & 3]++;
        per_ms = per / 1e6;
        uint64_t tg = target_at[id & 255];
        atomic_fetch_add_explicit(&n_taken, 1, memory_order_relaxed);
        if (tg && t > tg + per / 3) atomic_fetch_add_explicit(&n_late, 1, memory_order_relaxed);
    } else per = 0;
    if (prev && shown > prev) {
        uint64_t d = shown - prev;
        n++;
        if (per) {
            uint64_t k = (d + per / 2) / per, want_k = (want + per / 2) / per;
            by_vb[k < 1 ? 0 : k > 3 ? 3 : k]++;
            off += k != want_k;
            atomic_fetch_add_explicit(&n_cad, 1, memory_order_relaxed);
            if (k != want_k) atomic_fetch_add_explicit(&n_cad_off, 1, memory_order_relaxed);
        } else if (d * 4 < want * 3 || d * 4 > want * 5) off++;
        if (!min || d < min) min = d;
        if (d > max) max = d;
        if (verbose) {
            extern void slow_totals(uint64_t *uploads, uint64_t *bytes, uint64_t *pipes);  // (slowframe.c)
            static uint64_t lu, lb, lp;
            uint64_t u, by, pp;
            slow_totals(&u, &by, &pp);
            LOG("[present] frame %llu on screen %.2f ms after the one before (taken %.2f ms into a refresh, %.2f ms after "
                "its vkQueuePresentKHR; %llu uploads %.1f MB, %llu pipelines since)\n", (unsigned long long)id, d / 1e6,
                per && t > vb ? (t - vb) / 1e6 : -1.0, (t - queued_at[id & 255]) / 1e6, (unsigned long long)(u - lu),
                (by - lb) / 1048576.0, (unsigned long long)(pp - lp));
            lu = u, lb = by, lp = pp;
        }
        if (!t0) t0 = prev;
        if (shown - t0 >= 10000000000ull) {
            char vbs[160] = "";
            if (per_ms)
                snprintf(vbs, sizeof vbs, "; display %.2f Hz, frames held 1 / 2 / 3+ refreshes: %llu / %llu / %llu, taken "
                         "in each quarter of a refresh: %llu %llu %llu %llu", 1000.0 / per_ms, (unsigned long long)by_vb[1],
                         (unsigned long long)by_vb[2], (unsigned long long)by_vb[3], (unsigned long long)phase[0],
                         (unsigned long long)phase[1], (unsigned long long)phase[2], (unsigned long long)phase[3]);
            LOG("[present] %llu frames shown in %.1f s: %.1f ms apart (%.1f to %.1f), %llu off the %.1f ms cadence, %llu "
                "replaced%s\n", (unsigned long long)n, (shown - t0) / 1e9, (shown - t0) / 1e6 / (double)n, min / 1e6,
                max / 1e6, (unsigned long long)off, want / 1e6, (unsigned long long)replaced, vbs);
            t0 = shown, n = off = replaced = min = max = 0;
            memset(by_vb, 0, sizeof by_vb), memset(phase, 0, sizeof phase);
        }
    }
    if (id > id_before + 1) replaced += id - id_before - 1;
    prev = shown;
}

static void *waiter(void *arg) {
    (void)arg;
    plat_thread_realtime(4000000);  // (its wake-up is the time measured: late by a time slice would be noise)
    uint64_t next = 1;
    for (;;) {
        pthread_mutex_lock(&mu);
        while (atomic_load_explicit(&queued_pub, memory_order_acquire) < next) pthread_cond_wait(&cv, &mu);
        pthread_mutex_unlock(&mu);
        // waiting for an id returns once it or a later one is on the screen (with MAILBOX an older one may never be
        // shown: a newer took its place)
        uint64_t want = next;
        pthread_mutex_lock(&sc_mu);
        VkSwapchainKHR sc = wait_sc;
        VkResult r = sc ? wait_present(g_vk.dev, sc, want, 20000000ull) : VK_ERROR_OUT_OF_DATE_KHR;
        uint64_t t = plat_now_ns();
        uint64_t shown = want, top = atomic_load_explicit(&queued_pub, memory_order_acquire);
        // which id is on the screen now: newer ones already shown too? (probed without waiting)
        while (r == VK_SUCCESS && shown < top && wait_present(g_vk.dev, sc, shown + 1, 0) == VK_SUCCESS) shown++;
        pthread_mutex_unlock(&sc_mu);
        if (r == VK_SUCCESS) {
            stats(t, shown, next - 1);
            next = shown + 1;
        } else if (r != VK_TIMEOUT) {  // (timeout: not shown yet - minimised, occluded, a long frame)
            next = atomic_load_explicit(&queued_pub, memory_order_acquire) + 1;  // (swapchain gone: start over)
        }
    }
    return NULL;
}

// (device.c) the extensions are enabled: start
void present_timing_init(void) {
    wait_present = (PFN_vkWaitForPresentKHR)vkGetDeviceProcAddr(g_vk.dev, "vkWaitForPresentKHR");
    if (!wait_present) { g_vk.present_wait = false; return; }
    pthread_t t;
    pthread_create(&t, NULL, waiter, NULL);
    LOG("[vk] frames on the screen timed (VK_KHR_present_wait)%s\n", verbose ? ": each frame logged" : "");
}

// (record.c, the render thread) the id for this present (0: none); after vkQueuePresentKHR, present_timing_queued
uint64_t present_timing_next_id(void) { return g_vk.present_wait ? queued + 1 : 0; }
void present_timing_cadence(uint64_t *frames, uint64_t *off) {
    *frames = atomic_load_explicit(&n_cad, memory_order_relaxed);
    *off = atomic_load_explicit(&n_cad_off, memory_order_relaxed);
}
void present_timing_lateness(uint64_t *frames, uint64_t *late) {
    *frames = atomic_load_explicit(&n_taken, memory_order_relaxed);
    *late = atomic_load_explicit(&n_late, memory_order_relaxed);
}
void present_timing_queued(VkSwapchainKHR sc, uint64_t id, uint64_t target_ns) {
    if (!id) return;
    queued = id;
    queued_at[id & 255] = plat_now_ns();
    target_at[id & 255] = target_ns;
    if (wait_sc != sc) {  // (a new swapchain: set where no wait is on the old one)
        pthread_mutex_lock(&sc_mu);
        wait_sc = sc;
        pthread_mutex_unlock(&sc_mu);
    }
    pthread_mutex_lock(&mu);
    atomic_store_explicit(&queued_pub, id, memory_order_release);
    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&mu);
}

// (device.c) around a swapchain's destruction: no wait on it then; ids carry on (they are per device here)
void present_timing_swapchain_lock(void) {
    if (!g_vk.present_wait) return;
    pthread_mutex_lock(&sc_mu);
    wait_sc = VK_NULL_HANDLE;  // (the old one's ids won't be shown: the waiter starts over at the next present)
}
void present_timing_swapchain_unlock(void) { if (g_vk.present_wait) pthread_mutex_unlock(&sc_mu); }
