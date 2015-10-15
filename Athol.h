/*
 * Copyright (c) 2015, Igalia S.L.
 * Copyright (c) 2015, Metrological
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.

 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Athol_h
#define Athol_h

#include "Input.h"
#include <API/Interfaces.h>

#include "wayland-bcmrpi-dispmanx-server-protocol.h"
#include <wayland-server.h>

#define BUILD_WAYLAND
#include <bcm_host.h>

class Pointer;
class Surface;

class Athol final : public API::Compositor {
public:
    Athol(const char*);
    ~Athol();

    void run();
    void scheduleRepaint(Surface&);
    void scheduleReposition(Pointer&);

    class Update {
    public:
        Update(Athol&);
        ~Update();

        Update(const Update&) = delete;
        Update& operator=(const Update&) = delete;

        uint32_t width() { return m_athol.m_width; }
        uint32_t height() { return m_athol.m_height; }

        DISPMANX_UPDATE_HANDLE_T handle() { return m_updateHandle; }
        DISPMANX_DISPLAY_HANDLE_T displayHandle() { return m_athol.m_backend.displayHandle; }

    private:
        Athol& m_athol;
        DISPMANX_UPDATE_HANDLE_T m_updateHandle;
    };

    uint32_t width() { return m_width; }
    uint32_t height() { return m_height; }

    // API::Compositor
    virtual struct wl_display* display() const override;
    virtual void initializeInput(std::unique_ptr<API::InputClient>) override;

private:
    static void bindCompositorInterface(struct wl_client*, void*, uint32_t, uint32_t);
    static const struct wl_compositor_interface m_compositorInterface;
    static void bindBCMRPiDispmanxInterface(struct wl_client*, void*, uint32_t, uint32_t);
    static const struct wl_bcmrpi_dispmanx_interface m_BCMRPiDispmanxInterface;

    struct wl_display* m_display;
    bool m_initialized;

    struct wl_list m_surfaceUpdateList;
    struct wl_list m_pendingSurfaceUpdateList;
    struct wl_list m_pointerUpdateList;
    struct wl_list m_pendingPointerUpdateList;
    struct wl_event_source* m_vsyncSource;
    struct wl_event_source* m_repaintSource;
    int m_eventfd;
    bool m_updateInProgress;

    void scheduleRepaintSource();
    static void repaint(void*);
    static int vsyncCallback(int, uint32_t, void*);
    static void updateComplete(DISPMANX_UPDATE_HANDLE_T, void*);

    uint32_t m_width;
    uint32_t m_height;

    struct RPiBackend {
        DISPMANX_DISPLAY_HANDLE_T displayHandle;
    } m_backend;

    Input m_input;
};

#endif // Athol_h
