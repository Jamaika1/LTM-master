// The copyright in this software is being made available under the BSD
// License, included below. This software may be subject to other third party
// and contributor rights, including patent rights, and no such rights are
// granted under this license.
//
// Copyright (c) 2022, ISO/IEC
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//  * Neither the name of the ISO/IEC nor the names of its contributors may
//    be used to endorse or promote products derived from this software without
//    specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.
//
//
// Contributors: Sam Littlewood (sam.littlewood@v-nova.com)
//
// RingBuffer.hpp
//
// Fixed size thread safe ring buffer
//
#pragma once

#include <cassert>
#include <condition_variable>
#include <mutex>
#include <vector>

template <typename T> class RingBuffer {
public:
	typedef T value_type;

	RingBuffer(int capacity) : capacity_(capacity), ring_(capacity) {}

	// Push new datum into ring, blocking if buffer is full
	void push(T const &v) {
		std::unique_lock<std::mutex> lock(mutex_);
		while (ring_full())
			not_full_.wait(lock);
		ring_push_back(v);
		lock.unlock();
		not_empty_.notify_all(); // So that all 'pop_if's get a chance
	}

	// Push new datum into ring, popping from front if buffer is still full after timeout
	// Takes a destination for any lost element, so that it may be destroyed safely by client
	// return true if there was space
	bool push_timeout(T const &v, T &overflow, unsigned int usec = 0) {
		std::unique_lock<std::mutex> lock(mutex_);
		bool r = true;
		while (ring_full()) {
			if (usec == 0 || not_full_.wait_for(lock, std::chrono::microseconds(usec)) == std::cv_status::timeout) {
				overflow = ring_front();
				ring_pop_front();
				r = false;
				break;
			}
		}
		ring_push_back(v);
		lock.unlock();
		not_empty_.notify_all(); // So that all 'pop_if's get a chance
		return r;
	}

	// Retrieve a datum from ring - block until one is available
	void pop(T &v) {
		std::unique_lock<std::mutex> lock(mutex_);
		while (ring_empty())
			not_empty_.wait(lock);

		v = ring_front();
		ring_pop_front();
		lock.unlock();
		not_full_.notify_one();
	}

	// Pop - waiting for given timeout (in microseconds).
	// A timeout of 0 will return immediately
	//
	// Return true if there was one
	//
	bool pop_timeout(T &v, unsigned int usec = 0) {
		std::unique_lock<std::mutex> lock(mutex_);
		while (ring_empty())
			if (usec == 0 || not_empty_.wait_for(lock, std::chrono::microseconds(usec)) == std::cv_status::timeout)
				return false;

		v = ring_front();
		ring_pop_front();
		lock.unlock();
		not_full_.notify_one();
		return true;
	}

	// Retrieve a datum from ring , and predicate is true. - block until one is available
	template <typename P> void pop_if(T &v, const P &p) {
		std::unique_lock<std::mutex> lock(mutex_);
		while (ring_empty() || !p(ring_front()))
			not_empty_.wait(lock);

		v = ring_front();
		ring_pop_front();
		lock.unlock();
		not_full_.notify_one();
	}

	// Pop front if predicate return true  - waitimg for given timeout (in microseconds).
	// A timeout of 0 will return immediately
	//
	// Return true if there was one
	//
	template <typename P> bool pop_if_timeout(T &v, const P &pr, unsigned int usec = 0) {
		std::unique_lock<std::mutex> lock(mutex_);
		while (ring_empty() || !pr(ring_front()))
			if (usec == 0 || not_empty_.wait_for(lock, std::chrono::microseconds(usec)) == std::cv_status::timeout)
				return false;

		v = ring_front();
		ring_pop_front();
		lock.unlock();
		not_full_.notify_one();
		return true;
	}

	// Look at top element
	void peek(T &v) {
		std::unique_lock<std::mutex> lock(mutex_);
		while (ring_empty())
			not_empty_.wait(lock);

		v = ring_front();
	}

	// Destroy contents of ring
	void clear() {
		std::unique_lock<std::mutex> lock(mutex_);
		ring_clear();
		not_full_.notify_all();
	}

	// Test if ring is empty
	bool empty() const {
		std::unique_lock<std::mutex> lock(mutex_);
		return ring_empty();
	}

	// Test if ring is full
	bool full() const {
		std::unique_lock<std::mutex> lock(mutex_);
		return ring_full();
	}

	// Return amount of space remaining in buffer
	unsigned int space() const {
		std::unique_lock<std::mutex> lock(mutex_);
		return static_cast<unsigned int>(capacity_ - ring_size());
	}

	// Return number of entries in buffer
	unsigned int size() const {
		std::unique_lock<std::mutex> lock(mutex_);
		return static_cast<unsigned int>(ring_size());
	}

private:
	//// Internal ring primitives
	//
	void ring_pop_front() {
		assert(size_ > 0);

		// Clear slot
		T t;
		std::swap(ring_[front_], t);

		// Move indices on
		size_--;
		front_++;
		if (front_ == capacity_)
			front_ = 0;
	}

	void ring_push_back(const T &v) {
		assert(size_ <= capacity_);

		// Find back slot
		size_t b = front_ + size_;
		if (b >= capacity_)
			b -= capacity_;

		// Store
		ring_[b] = v;
		size_++;
	}

	void ring_clear() {
		while (size_ > 0)
			ring_pop_front();
	}

	T ring_front() const {
		assert(size_ > 0);
		return ring_[front_];
	}

	size_t ring_size() const { return size_; }

	bool ring_full() const { return size_ == capacity_; }

	bool ring_empty() const { return size_ == 0; }

	//// State
	//
	// Fixed capacity of ring buffer
	size_t capacity_;

	// The ring
	std::vector<T> ring_;

	// Index to front of ring
	unsigned int front_ = 0;

	// Current size
	unsigned int size_ = 0;

	// Threading
	mutable std::mutex mutex_;
	std::condition_variable not_empty_;
	std::condition_variable not_full_;
};
