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

Overlay::Overlay(overlay_set_fd_hook set_fd,
        overlay_set_crop_hook set_crop,
        overlay_queue_buffer_hook queue_buffer,
        void *data)
    : mStatus(NO_INIT)
{
    LOGD("%s: Init overlay", __FUNCTION__);
    set_fd_hook = set_fd;
    set_crop_hook = set_crop;
    queue_buffer_hook = queue_buffer;
    hook_data = data;
    mStatus = NO_ERROR;
    
    int fd = ashmem_create_region("Overlay_buffer_region", NUM_BUFFERS * BUFFER_SIZE);
    if (fd < 0) {
	LOGE("%s: Cannot create ashmem region", __FUNCTION__);
	return;
    }
    mBuffers = new mapping_data_t[NUM_BUFFERS];
    for(int i=0; i<NUM_BUFFERS; i++) {
	mBuffers[i].fd = fd;
	mBuffers[i].length = BUFFER_SIZE;
	mBuffers[i].offset = BUFFER_SIZE * i;
	mBuffers[i].ptr = mmap(NULL, BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, BUFFER_SIZE * i);
	if (mBuffers[i].ptr == MAP_FAILED) {
	    LOGE("%s: Failed to mmap buffer %d", __FUNCTION__, i);
	}
    }
    LOGD("%s: Init overlay complete", __FUNCTION__);
}

Overlay::~Overlay() {
}

status_t Overlay::dequeueBuffer(void** buffer)
{
    LOGD("%s: %p", __FUNCTION__, buffer);
    return mStatus;
}

status_t Overlay::queueBuffer(void* buffer)
{
    LOGD("%s: %p", __FUNCTION__, buffer);
    if (queue_buffer_hook)
        queue_buffer_hook(hook_data, buffer); // TODO buffer shall not be used by hook, it's just a number
    return mStatus;
}

status_t Overlay::resizeInput(uint32_t width, uint32_t height)
{
    LOGD("%s: %d, %d", __FUNCTION__, width, height);
    return mStatus;
}

status_t Overlay::setParameter(int param, int value)
{
    LOGD("%s: %d, %d", __FUNCTION__, param, value);
    return mStatus;
}

status_t Overlay::setCrop(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    LOGD("%s: x=%d, y=%d, w=%d, h=%d", __FUNCTION__, x, y, w, h);
    if (set_crop_hook) {
	LOGD("%s: Invoking crop hook", __FUNCTION__);
        set_crop_hook(hook_data, x, y, w, h);
    }
    return mStatus;
}

status_t Overlay::getCrop(uint32_t* x, uint32_t* y, uint32_t* w, uint32_t* h)
{
    LOGD("%s", __FUNCTION__);
    return mStatus;
}

status_t Overlay::setFd(int fd)
{
    LOGD("%s: fd=%d", __FUNCTION__, fd);
    if (set_fd_hook)
        set_fd_hook(hook_data, fd);
    return mStatus;
}

int32_t Overlay::getBufferCount() const
{
    LOGD("%s: %d", __FUNCTION__, NUM_BUFFERS);
    return NUM_BUFFERS;
}

void* Overlay::getBufferAddress(void* buffer)
{
    LOGD("%s: %d", __FUNCTION__, (int)buffer);
    int index = (int) buffer;
    if (index <0 || index >= NUM_BUFFERS) {
	return NULL;
    }
    
    //LOGD("%s: fd=%d, length=%d. offset=%d, ptr=%p", __FUNCTION__, mBuffers[index].fd, mBuffers[index].length, mBuffers[index].offset, mBuffers[index].ptr);
    
    return &mBuffers[index];
}

void Overlay::destroy() {
    LOGD("%s", __FUNCTION__);
    for(int i=0; i<NUM_BUFFERS; i++) {
        if( munmap(mBuffers[i].ptr, mBuffers[i].length) < 0) {
	    LOGD("%s: unmap of buffer %d failed", __FUNCTION__, i);
	}
    }
    
    delete[] mBuffers;
}

status_t Overlay::getStatus() const {
    LOGD("%s", __FUNCTION__);
    return mStatus;
}

void* Overlay::getHandleRef() const {
    LOGD("%s", __FUNCTION__);
    return 0;
}

uint32_t Overlay::getWidth() const {
    LOGD("%s", __FUNCTION__);
    return 0;
}

uint32_t Overlay::getHeight() const {
    LOGD("%s", __FUNCTION__);
    return 0;
}

int32_t Overlay::getFormat() const {
    LOGD("%s", __FUNCTION__);
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
