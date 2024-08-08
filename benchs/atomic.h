
#include <cstdint>
#include <atomic>

#define ALIGNED_STRUCT(name, alignment)  struct name


ALIGNED_STRUCT(atomicptr_t, 8) {
	std::atomic<void*> nonatomic;
};
typedef struct atomicptr_t atomicptr_t;

static void*
atomic_load_ptr(atomicptr_t* src) {
	return src->nonatomic.load(std::memory_order_relaxed);
}

static void
atomic_store_ptr(atomicptr_t* dst, void* val) {
	dst->nonatomic = val;
}

ALIGNED_STRUCT(atomic32_t, 4) {
	std::atomic<int32_t> nonatomic;
};
typedef struct atomic32_t atomic32_t;

static int32_t
atomic_load32(atomic32_t* src) {
	return src->nonatomic.load(std::memory_order_relaxed);
}

static void
atomic_store32(atomic32_t* dst, int32_t val) {
	dst->nonatomic = val;
}

static int32_t
atomic_incr32(atomic32_t* val) {
	return val->nonatomic.fetch_add(1) + 1;
}

static int32_t
atomic_decr32(atomic32_t* val) {
	return val->nonatomic.fetch_add(-1) - 1;
}

static int32_t
atomic_add32(atomic32_t* val, int32_t add) {
	return val->nonatomic.fetch_add(add) + add;
}

static int
atomic_cas_ptr(atomicptr_t* dst, void* val, void* ref) {
	return dst->nonatomic.compare_exchange_strong(ref, val);

}
