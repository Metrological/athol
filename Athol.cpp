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

#include "Athol.h"

#include "Pointer.h"
#include "Surface.h"
#include <cstdio>
#include <cstdlib>
#include <sys/eventfd.h>
#include <sys/time.h>

Athol::Athol(const char* socketName)
    : m_display(wl_display_create())
    , m_initialized(false)
    , m_updateInProgress(false)
    , m_input(*this)
{
    wl_display_add_socket(m_display, socketName);
    setenv("WAYLAND_DISPLAY", socketName, 1);

    if (!wl_global_create(m_display, &wl_compositor_interface, 3, this, bindCompositorInterface))
        return;
    if (!wl_global_create(m_display, &wl_bcmrpi_dispmanx_interface, 1, this, bindBCMRPiDispmanxInterface))
        return;

    wl_list_init(&m_surfaceUpdateList);
    wl_list_init(&m_pendingSurfaceUpdateList);
    wl_list_init(&m_pointerUpdateList);
    wl_list_init(&m_pendingPointerUpdateList);

    m_eventfd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (m_eventfd == -1)
        return;

    m_vsyncSource = wl_event_loop_add_fd(wl_display_get_event_loop(m_display),
        m_eventfd, WL_EVENT_READABLE, vsyncCallback, this);
    m_repaintSource = nullptr;

    bcm_host_init();

    m_backend.displayHandle = vc_dispmanx_display_open(DISPMANX_ID_HDMI);

    graphics_get_display_size(DISPMANX_ID_HDMI, &m_width, &m_height);

    m_initialized = true;
}

Athol::~Athol()
{
    wl_display_destroy(m_display);
    vc_dispmanx_display_close(m_backend.displayHandle);
}

void Athol::run()
{
    wl_display_run(m_display);
}

void Athol::scheduleRepaint(Surface& surface)
{
    if (m_updateInProgress) {
        wl_list_insert(m_pendingSurfaceUpdateList.prev, &surface.link);
        return;
    }

    Surface* listedSurface;
    wl_list_for_each(listedSurface, &m_pointerUpdateList, link) {
        if (&surface == listedSurface)
            return;
    }

    wl_list_insert(m_surfaceUpdateList.prev, &surface.link);
    scheduleRepaintSource();
}

void Athol::scheduleReposition(Pointer& pointer)
{
    if (m_updateInProgress) {
        wl_list_insert(m_pendingPointerUpdateList.prev, &pointer.link);
        return;
    }

    Pointer* listedPointer;
    wl_list_for_each(listedPointer, &m_pointerUpdateList, link) {
        if (&pointer == listedPointer)
            return;
    }

    wl_list_insert(m_pointerUpdateList.prev, &pointer.link);
    scheduleRepaintSource();
}

void Athol::scheduleRepaintSource()
{
    if (!m_repaintSource)
        m_repaintSource = wl_event_loop_add_idle(
            wl_display_get_event_loop(m_display), Athol::repaint, this);
}

void Athol::repaint(void* data)
{
    auto& athol = *static_cast<Athol*>(data);
    athol.m_updateInProgress = true;
    athol.m_repaintSource = nullptr;

    Athol::Update update(athol);

    Surface* surface;
    wl_list_for_each(surface, &athol.m_surfaceUpdateList, link)
        surface->repaint(update);

    Pointer* pointer;
    wl_list_for_each(pointer, &athol.m_pointerUpdateList, link)
        pointer->reposition(update);
}

int Athol::vsyncCallback(int fd, uint32_t mask, void* data)
{
    if (mask != WL_EVENT_READABLE)
        return 1;

    Athol& athol = *static_cast<Athol*>(data);

    uint64_t time;
    ssize_t ret = read(fd, &time, sizeof(time));
    if (ret != sizeof(time))
        return 1;

    Surface* surface;
    wl_list_for_each(surface, &athol.m_surfaceUpdateList, link)
        surface->dispatchFrameCallbacks(time);

    athol.m_updateInProgress = false;
    wl_list_init(&athol.m_surfaceUpdateList);
    wl_list_init(&athol.m_pointerUpdateList);

    bool scheduleRepaintSource = false;

    if (!wl_list_empty(&athol.m_pendingSurfaceUpdateList)) {
        wl_list_insert_list(&athol.m_surfaceUpdateList, &athol.m_pendingSurfaceUpdateList);
        wl_list_init(&athol.m_pendingSurfaceUpdateList);

        scheduleRepaintSource = true;
    }

    if (!wl_list_empty(&athol.m_pendingPointerUpdateList)) {
        wl_list_insert_list(&athol.m_pointerUpdateList, &athol.m_pendingPointerUpdateList);
        wl_list_init(&athol.m_pendingPointerUpdateList);

        scheduleRepaintSource = true;
    }

    if (scheduleRepaintSource)
        athol.scheduleRepaintSource();

    return 1;
}

void Athol::updateComplete(DISPMANX_UPDATE_HANDLE_T, void* data)
{
    Athol& athol = *static_cast<Athol*>(data);

    struct timeval tv;
    gettimeofday(&tv, nullptr);
    uint64_t time = tv.tv_sec * 1000 + tv.tv_usec / 1000;

    ssize_t ret = write(athol.m_eventfd, &time, sizeof(time));
    if (ret != sizeof(time))
        return; // FIXME: At least log this.
}

void Athol::bindCompositorInterface(struct wl_client* client, void* data, uint32_t version, uint32_t id)
{
    struct wl_resource* resource = wl_resource_create(client, &wl_compositor_interface, 3, id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }

    wl_resource_set_implementation(resource, &m_compositorInterface, data, nullptr);
}

const struct wl_compositor_interface Athol::m_compositorInterface = {
    // create_surface
    [](struct wl_client* client, struct wl_resource* resource, uint32_t id)
    {
        auto& athol = *static_cast<Athol*>(wl_resource_get_user_data(resource));
        new Surface(athol, client, resource, id);
    },
    // create_region
    [](struct wl_client*, struct wl_resource*, uint32_t)
    {
        std::fprintf(stderr, "m_compositorInterface::create_region not implemented\n");
    }
};

void Athol::bindBCMRPiDispmanxInterface(struct wl_client* client, void* data, uint32_t version, uint32_t id)
{
    struct wl_resource* resource = wl_resource_create(client, &wl_bcmrpi_dispmanx_interface, 1, id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }

    wl_resource_set_implementation(resource, &m_BCMRPiDispmanxInterface, data, nullptr);
}

const struct wl_bcmrpi_dispmanx_interface Athol::m_BCMRPiDispmanxInterface = {
    // create_element
    [](struct wl_client*, struct wl_resource* resource, struct wl_resource* surfaceResource, uint32_t width, uint32_t height)
    {
        auto& surface = *static_cast<Surface*>(wl_resource_get_user_data(surfaceResource));
        wl_bcmrpi_dispmanx_send_element_created(resource, surfaceResource, surface.createElement());
    },
};

Athol::Update::Update(Athol& athol)
    : m_athol(athol)
{
    m_updateHandle = vc_dispmanx_update_start(10);
}

Athol::Update::~Update()
{
    vc_dispmanx_update_submit(m_updateHandle, Athol::updateComplete, &m_athol);
}

struct wl_display* Athol::display() const
{
    return m_display;
}

void Athol::initializeInput(std::unique_ptr<API::InputClient> client)
{
    m_input.initialize(std::move(client));
}
