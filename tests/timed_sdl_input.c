// Wall-clock SDL input injection for the independent latency acceptance test.
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <SDL.h>
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static struct timespec epoch;
static _Atomic int started;
static unsigned frames, pause_frame, pause_ms;

static double elapsed(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec - epoch.tv_sec) + (now.tv_nsec - epoch.tv_nsec)/1e9;
}

static void *inject(void *unused)
{
    (void)unused;
    const char *path = getenv("X4_QA_TIMED_INPUT");
    FILE *file = path ? fopen(path, "r") : NULL;
    if (!file) { fprintf(stderr, "QA_ERROR cannot open timed input\n"); return NULL; }
    unsigned ms;
    char name[32], action[16];
    while (fscanf(file, "%u %31s %15s", &ms, name, action) == 3) {
        struct timespec at = epoch;
        at.tv_sec += ms/1000;
        at.tv_nsec += (ms%1000)*1000000L;
        if (at.tv_nsec >= 1000000000L) { at.tv_sec++; at.tv_nsec -= 1000000000L; }
        while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &at, NULL) == EINTR) {}
        SDL_Event event = {0};
        event.common.timestamp = SDL_GetTicks();
        if (!strcmp(name, "quit")) event.type = SDL_QUIT;
        else {
            event.type = !strcmp(action, "down") ? SDL_KEYDOWN : SDL_KEYUP;
            event.key.state = event.type == SDL_KEYDOWN ? SDL_PRESSED : SDL_RELEASED;
            event.key.keysym.sym = SDL_GetKeyFromName(name);
            event.key.keysym.scancode = SDL_GetScancodeFromKey(event.key.keysym.sym);
        }
        fprintf(stderr, "QA_INPUT %.6f %s %s\n", elapsed(), name, action);
        if (SDL_PushEvent(&event) != 1) fprintf(stderr, "QA_ERROR SDL_PushEvent failed\n");
    }
    fclose(file);
    return NULL;
}

int SDL_PollEvent(SDL_Event *event)
{
    static int (*original)(SDL_Event *);
    if (!original) original = dlsym(RTLD_NEXT, "SDL_PollEvent");
    if (!atomic_exchange(&started, 1)) {
        clock_gettime(CLOCK_MONOTONIC, &epoch);
        const char *frame = getenv("X4_QA_PAUSE_FRAME"), *ms = getenv("X4_QA_PAUSE_MS");
        pause_frame = frame ? (unsigned)strtoul(frame, NULL, 10) : 0;
        pause_ms = ms ? (unsigned)strtoul(ms, NULL, 10) : 0;
        pthread_t thread;
        if (pthread_create(&thread, NULL, inject, NULL)) fprintf(stderr, "QA_ERROR cannot start injector\n");
        else pthread_detach(thread);
    }
    return original(event);
}

void SDL_RenderPresent(SDL_Renderer *renderer)
{
    static void (*original)(SDL_Renderer *);
    if (!original) original = dlsym(RTLD_NEXT, "SDL_RenderPresent");
    if (atomic_load(&started)) {
        frames++;
        fprintf(stderr, "QA_FRAME %.6f %u\n", elapsed(), frames);
    }
    original(renderer);
}

void SDL_Delay(Uint32 ms)
{
    static void (*original)(Uint32);
    if (!original) original = dlsym(RTLD_NEXT, "SDL_Delay");
    // Only extend microreader's refresh delay, never the app's short idle waits.
    if (ms == 400 && pause_frame && frames == pause_frame) {
        fprintf(stderr, "QA_PAUSE %.6f %u\n", elapsed(), pause_ms);
        if (pause_ms > ms) ms = pause_ms;
        pause_frame = 0;
    }
    original(ms);
}

SDL_TimerID SDL_AddTimer(Uint32 ms, SDL_TimerCallback callback, void *param)
{
    static SDL_TimerID (*original)(Uint32, SDL_TimerCallback, void *);
    if (!original) original=dlsym(RTLD_NEXT,"SDL_AddTimer");
    if (ms == 400 && pause_frame && frames == pause_frame) {
        fprintf(stderr,"QA_PAUSE %.6f %u\n",elapsed(),pause_ms);
        if (pause_ms>ms) ms=pause_ms;
        pause_frame=0;
    }
    return original(ms,callback,param);
}
