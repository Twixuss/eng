#pragma once
#include "common.h"
#include <vector>
template <class T, size_t blockCapacity>
struct BlockAllocator {
	struct Iterator;
	union Storage {
		T val;
#pragma warning(suppress : 4582)
		Storage() {}
		~Storage() {}
	};
	struct Block {
		Storage storage[blockCapacity];
		u32 occupancy[max(blockCapacity / 32, size_t(1))]{};
		Block* nextBlock = 0;
		bool isOccupied(u32 idx) { return occupancy[idx >> 5] >> (idx & 31); }
		bool isOccupied(Storage* ptr) { return isOccupied(u32(ptr - storage)); }
		u32 findAndInvertOccupancy(bool v) {
			if (v) {
				for (u32 i = 0; i < _countof(occupancy); ++i) {
					u32 mask = 1;
					for (u32 j = 0; j < 32; ++j) {
						if (occupancy[i] & mask) {
							occupancy[i] &= ~mask;
							return i * 32 + j;
						}
						mask <<= 1;
					}
				}
			} else {
				for (u32 i = 0; i < _countof(occupancy); ++i) {
					u32 mask = 1;
					for (u32 j = 0; j < 32; ++j) {
						if (!(occupancy[i] & mask)) {
							occupancy[i] |= mask;
							return i * 32 + j;
						}
						mask <<= 1;
					}
				}
			}
			return (u32)-1;
		}
		T* allocate() {
			if (u32 idx = findAndInvertOccupancy(0); idx == -1)
				return 0;
			else
				return &storage[idx].val;
		}
		bool contains(Storage* s) { return storage <= s && s < storage + blockCapacity; }
		void deallocate(Storage* s) {
			s->val.~T();
			u32 idx = u32(s - storage);
			occupancy[idx >> 5] &= ~(u32(1) << u32(idx & 31));
		}
		u32 getFirstOccupiedIndex() {
			for (u32 i = 0; i < _countof(occupancy); ++i) {
				u32 mask = 1;
				for (u32 j = 0; j < 32; ++j) {
					if (occupancy[i] & mask) {
						return i * 32 + j;
					}
					mask <<= 1;
				}
			}
			return 0;
		}
		u32 getLastOccupiedIndex() {
			for (u32 i = _countof(occupancy) - 1;; --i) {
				u32 mask = u32(1 << 31);
				for (u32 j = 31;; --j) {
					if (occupancy[i] & mask) {
						return i * 32 + j;
					}
					mask >>= 1;
					if (j == 0) {
						break;
					}
				}
				if (i == 0) {
					break;
				}
			}
			return 0;
		}
		Storage* getFirstOccupiedItem() { return storage + getFirstOccupiedIndex(); }
		Storage* getLastOccupiedItem() { return storage + getLastOccupiedIndex(); }
		Iterator begin() { return {getFirstOccupiedItem(), this}; }
		Iterator end() { return {getLastOccupiedItem() + 1, this}; }
	};
	struct Iterator {
		Storage* val = 0;
		Block* block = 0;
		T& operator*() { return val->val; }
		T* operator->() { return &val->val; }
		Iterator& operator++() {
			do {
				++val;
				if (val > block->getLastOccupiedItem()) {
					block = block->nextBlock;
					if (block) {
						val = block->getFirstOccupiedItem();
					} else {
						break;
					}
				}
			} while (!block->isOccupied(val));
			return *this;
		}
		Iterator operator++(int) {
			Iterator r = *this;
			++*this;
			return r;
		}
		bool operator==(Iterator b) { return val == b.val; }
		bool operator!=(Iterator b) { return val != b.val; }
		bool operator<=(Iterator b) { return val <= b.val; }
		bool operator>=(Iterator b) { return val >= b.val; }
		bool operator<(Iterator b) { return val < b.val; }
		bool operator>(Iterator b) { return val > b.val; }
	};

	template <class... Args>
	T* allocate(Args&&... args) {
		auto r = lastBlock->allocate();
		if (!r) {
			lastBlock->nextBlock = new Block;
			lastBlock = lastBlock->nextBlock;
			r = lastBlock->allocate();
			ASSERT(r);
		}
		return new (r) T(std::forward<Args>(args)...);
	}
	void deallocate(T* t) {
		Block* b = &firstBlock;
		while (!b->contains((Storage*)t)) {
			b = b->nextBlock;
			ASSERT(b);
		}
		b->deallocate((Storage*)t);
	}
	void clear() {
		INVALID_CODE_PATH("not implemented");
	}
	Iterator begin() { return firstBlock.begin(); }
	Iterator end() { return lastBlock->end(); }

private:
	Block firstBlock;
	Block* lastBlock = &firstBlock;
};
#if 0
template <class T, size_t blockCapacity>
struct BlockAllocatorL {
	struct Iterator;
	union Storage {
		T val;
		Storage() {}
		~Storage() {}
	};
	struct Block {
		struct Node {
			Node* prev;
			Node* next;
		};
		Storage storage[blockCapacity];
		Node occupancy[blockCapacity]{};
		Iterator begin() {
			for (auto o : occupancy) {
				if (o) {
					return {o, this};
				}
			}
		}
	};
	struct Iterator {
		Storage* val = 0;
		Block* block = 0;
		T& operator*() { return val->val; }
		T* operator->() { return &val->val; }
		Iterator& operator++() {
			INVALID_CODE_PATH("not implemented");
			return *this;
		}
		Iterator operator++(int) {
			Iterator r = *this;
			++*this;
			return r;
		}
		bool operator==(Iterator b) { return val == b.val; }
		bool operator!=(Iterator b) { return val != b.val; }
		bool operator<=(Iterator b) { return val <= b.val; }
		bool operator>=(Iterator b) { return val >= b.val; }
		bool operator<(Iterator b) { return val < b.val; }
		bool operator>(Iterator b) { return val > b.val; }
	};
	template <class... Args>
	T* allocate(Args&&... args) {}
	void deallocate(T* t) {}
	Iterator begin() { return firstBlock.begin(); }
	Iterator end() { return lastBlock->end(); }

private:
	Block firstBlock;
	Block* lastBlock = &firstBlock;
};
#endif
template <class T>
using Allocator = BlockAllocator<T, 256>;
