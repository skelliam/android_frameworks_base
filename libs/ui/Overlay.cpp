/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "Overlay"

#include <binder/IMemory.h>
#include <binder/Parcel.h>
#include <utils/Errors.h>
#include <binder/MemoryHeapBase.h>
#include <cutils/ashmem.h>

#include <ui/Overlay.h>

namespace android {

Overlay::Overlay(uint32_t width, uint32_t height, overlay_queue_buffer_hook queue_buffer, void *hook_data) : mStatus(NO_INIT) {
    LOGD("%s: Init overlay", __FUNCTION__);
    queue_buffer_hook = queue_buffer;
    this->hook_data = hook_data;
    this->width = width;
    this->height = height;
    numFreeBuffers = NUM_BUFFERS;

    const int BUFFER_SIZE = width * height * 4;

    int fd = ashmem_create_region("Overlay_buffer_region", NUM_BUFFERS * BUFFER_SIZE);
    if (fd < 0) {
        LOGE("%s: Cannot create ashmem region", __FUNCTION__);
        return;
    }

    mBuffers = new mapping_data_t[NUM_BUFFERS];
    mQueued = new bool[NUM_BUFFERS];
    for(uint32_t i=0; i<NUM_BUFFERS; i++) {
        mBuffers[i].fd = fd;
        mBuffers[i].length = BUFFER_SIZE;
        mBuffers[i].offset = BUFFER_SIZE * i;
        mBuffers[i].ptr = mmap(NULL, BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, BUFFER_SIZE * i);
        if (mBuffers[i].ptr == MAP_FAILED) {
            LOGE("%s: Failed to mmap buffer %d", __FUNCTION__, i);
        }
        mQueued[i]=false;
    }

    pthread_mutex_init(&queue_mutex, NULL);

    LOGD("%s: Init overlay complete", __FUNCTION__);

    mStatus = NO_ERROR;
}

Overlay::~Overlay() {
}

status_t Overlay::dequeueBuffer(overlay_buffer_t* buffer)
{
    LOGD("%s: %d", __FUNCTION__, (int)*buffer);
    int rv = 0;
    pthread_mutex_lock(&queue_mutex);

    if (numFreeBuffers < 1) {
        LOGV("%s: No free buffers", __FUNCTION__);
        rv = -EPERM;
    } else {
        int index = -1;
        for(uint32_t i = 0; i < NUM_BUFFERS; i++) {
            if (mQueued[i] == false) {
                mQueued[i] = true;
                index = i;
                break;
            }
        }

        if (index >= 0) {
            *((int*)buffer) = index;
            numFreeBuffers--;
            LOGV("%s: dequeued", __FUNCTION__);
        } else {
            LOGE("%s: inconsistent queue state", __FUNCTION__);
            rv = -EPERM;
        }
    }

    pthread_mutex_unlock(&queue_mutex);
    return rv;
}

status_t Overlay::queueBuffer(overlay_buffer_t buffer)
{
    LOGD("%s: %d", __FUNCTION__, (int)buffer);
    if ((uint32_t)buffer > NUM_BUFFERS) {
        LOGE("%s: invalid buffer index %d", __FUNCTION__, (uint32_t) buffer);
        return -1;
    }

    if (queue_buffer_hook) {
        queue_buffer_hook(hook_data, mBuffers[(int)buffer].ptr, width*height*4);
    }

    pthread_mutex_lock(&queue_mutex);

    if (numFreeBuffers < NUM_BUFFERS) {
         numFreeBuffers++;
         mQueued[(int)buffer] = false;
    }

    int rv = numFreeBuffers;

    pthread_mutex_unlock(&queue_mutex);
    LOGV("%s: numFreeBuffers=%d", __FUNCTION__, rv);
    return rv;
}

status_t Overlay::resizeInput(uint32_t width, uint32_t height)
{
    LOGW("%s: %d, %d", __FUNCTION__, width, height);
    return mStatus;
}

status_t Overlay::setParameter(int param, int value)
{
    LOGW("%s: %d, %d", __FUNCTION__, param, value);
    return mStatus;
}

status_t Overlay::setCrop(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    LOGW("%s: x=%d, y=%d, w=%d, h=%d", __FUNCTION__, x, y, w, h);
    return mStatus;
}

status_t Overlay::getCrop(uint32_t* x, uint32_t* y, uint32_t* w, uint32_t* h)
{
    LOGW("%s", __FUNCTION__);
    return mStatus;
}

status_t Overlay::setFd(int fd)
{
    LOGW("%s: fd=%d", __FUNCTION__, fd);
    return mStatus;
}

int32_t Overlay::getBufferCount() const
{
    LOGV("%s: %d", __FUNCTION__, NUM_BUFFERS);
    return NUM_BUFFERS;
}

void* Overlay::getBufferAddress(overlay_buffer_t buffer)
{
    LOGD("%s: %d", __FUNCTION__, (int)buffer);
    if ((uint32_t)buffer >= NUM_BUFFERS) {
        return NULL;
    }

    //LOGD("%s: fd=%d, length=%d. offset=%d, ptr=%p", __FUNCTION__, mBuffers[buffer].fd, mBuffers[buffer].length, mBuffers[buffer].offset, mBuffers[buffer].ptr);

    return &mBuffers[(uint32_t)buffer];
}

void Overlay::destroy() {
    LOGD("%s", __FUNCTION__);
    for(uint32_t i=0; i<NUM_BUFFERS; i++) {
        if( munmap(mBuffers[i].ptr, mBuffers[i].length) < 0) {
            LOGD("%s: unmap of buffer %d failed", __FUNCTION__, i);
        }
    }

    delete[] mBuffers;
    delete[] mQueued;
    pthread_mutex_destroy(&queue_mutex);
}

status_t Overlay::getStatus() const {
    LOGV("%s", __FUNCTION__);
    return mStatus;
}

overlay_handle_t Overlay::getHandleRef() const {
    LOGW("%s", __FUNCTION__);
    return 0;
}

uint32_t Overlay::getWidth() const {
    LOGV("%s", __FUNCTION__);
    return width;
}

uint32_t Overlay::getHeight() const {
    LOGV("%s", __FUNCTION__);
    return height;
}

int32_t Overlay::getFormat() const {
    LOGW("%s", __FUNCTION__);
    return 0;
}

int32_t Overlay::getWidthStride() const {
    LOGD("%s", __FUNCTION__);
    return 0;
}

int32_t Overlay::getHeightStride() const {
    LOGD("%s", __FUNCTION__);
    return 0;
}

// ----------------------------------------------------------------------------

}; // namespace android
