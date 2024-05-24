#include "random.hpp"
#include "algorithm.hpp"
#include "cstring.hpp"
#include "event.hpp"
#include "manually_destroy.hpp"
#include "vector.hpp"

constexpr u32 rotate_left(u32 value, u8 num) {
	return (value << num) | (value >> (32 - num));
}

constexpr void chacha20_quarter_round(u32& a, u32& b, u32& c, u32& d) {
	a += b;
	d ^= a;
	d = rotate_left(d, 16);
	c += d;
	b ^= c;
	b = rotate_left(b, 12);
	a += b;
	d ^= a;
	d = rotate_left(d, 8);
	c += d;
	b ^= c;
	b = rotate_left(b, 7);
}

struct ChaCha20State {
	u32 state[16];

	constexpr void init(u32 (&key)[8], u32 block_counter, u32 (&nonce)[3]) {
		state[0] = 0x61707865;
		state[1] = 0x3320646E;
		state[2] = 0x79622D32;
		state[3] = 0x6B206574;
		state[4] = key[0];
		state[5] = key[1];
		state[6] = key[2];
		state[7] = key[3];
		state[8] = key[4];
		state[9] = key[5];
		state[10] = key[6];
		state[11] = key[7];
		state[12] = block_counter;
		state[13] = nonce[0];
		state[14] = nonce[1];
		state[15] = nonce[2];
	}

	constexpr void inner_block() {
		quarter_round(0, 4, 8, 12);
		quarter_round(1, 5, 9, 13);
		quarter_round(2, 6, 10, 14);
		quarter_round(3, 7, 11, 15);
		quarter_round(0, 5, 10, 15);
		quarter_round(1, 6, 11, 12);
		quarter_round(2, 7, 8, 13);
		quarter_round(3, 4, 9, 14);
	}

	constexpr void quarter_round(u32 x, u32 y, u32 z, u32 w) {
		chacha20_quarter_round(state[x], state[y], state[z], state[w]);
	}
};

constexpr ChaCha20State cha_cha20_block(u32 (&key)[8], u32 block_counter, u32 (&nonce)[3]) {
	ChaCha20State initial_state {};
	initial_state.init(key, block_counter, nonce);
	auto state = initial_state;
	for (int i = 0; i < 10; ++i) {
		state.inner_block();
	}

	for (int i = 0; i < 16; ++i) {
		state.state[i] += initial_state.state[i];
	}
	return state;
}

constexpr u64 rotate_right(u64 value, u8 num) {
	return (value >> num) | (value << (64 - num));
}

static constexpr u64 BLAKE2B_IV[8] {
	0x6A09E667F3BCC908, 0xBB67AE8584CAA73B,
	0x3C6EF372FE94F82B, 0xA54FF53A5F1D36F1,
	0x510E527FADE682D1, 0x9B05688C2B3E6C1F,
	0x1F83D9ABFB41BD6B, 0x5BE0CD19137E2179
};

struct Blake2bState {
	u64 input[16];
	u64 state[8];
	u64 offset[2];
	usize input_ptr;
	usize digest_size;

	constexpr void init(usize chosen_digest_size, const u64* key, usize key_words) {
		for (int i = 0; i < 8; ++i) {
			state[i] = BLAKE2B_IV[i];
		}
		state[0] ^= 0x01010000 ^ ((key_words * 8) << 8) ^ chosen_digest_size;
		offset[0] = 0;
		offset[1] = 0;
		input_ptr = 0;
		digest_size = chosen_digest_size;
		for (auto& value : input) {
			value = 0;
		}
		if (key_words) {
			add_data(key, key_words);
			input_ptr = 16;
		}
	}

	constexpr void add_data(const u64* data, usize words) {
		for (usize i = 0; i < words; ++i) {
			if (input_ptr == 16) {
				offset[0] += 128;
				if (offset[0] < 128) {
					offset[1] += 1;
				}

				compress(false);
				input_ptr = 0;
			}

			input[input_ptr++] = data[i];
		}
	}

	constexpr void generate_digest(u64* data) {
		offset[0] += input_ptr * 8;
		if (offset[0] < input_ptr * 8) {
			offset[1] += 1;
		}

		while (input_ptr < 16) {
			input[input_ptr++] = 0;
		}

		compress(true);

		for (usize i = 0; i < digest_size / 8; ++i) {
			data[i] = state[i];
		}
	}

private:
	constexpr void compress(bool final) {
		u64 v[16];

		for (int i = 0; i < 8; ++i) {
			v[i] = state[i];
			v[8 + i] = BLAKE2B_IV[i];
		}

		v[12] ^= offset[0];
		v[13] ^= offset[1];

		if (final) {
			v[14] = ~v[14];
		}

		constexpr u8 SIGMA[12][16] = {
			{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
			{14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3},
			{11, 8, 12, 0, 5, 2, 15, 13, 10, 14, 3, 6, 7, 1, 9, 4},
			{7, 9, 3, 1, 13, 12, 11, 14, 2, 6, 5, 10, 4, 0, 15, 8},
			{9, 0, 5, 7, 2, 4, 10, 15, 14, 1, 11, 12, 6, 8, 3, 13},
			{2, 12, 6, 10, 0, 11, 8, 3, 4, 13, 7, 5, 15, 14, 1, 9},
			{12, 5, 1, 15, 14, 13, 4, 10, 0, 7, 6, 3, 9, 2, 8, 11},
			{13, 11, 7, 14, 12, 1, 3, 9, 5, 0, 15, 4, 8, 6, 2, 10},
			{6, 15, 14, 9, 11, 3, 0, 8, 12, 2, 13, 7, 1, 4, 10, 5},
			{10, 2, 8, 4, 7, 6, 1, 5, 15, 11, 9, 14, 3, 12, 13, 0},
			{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
			{14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3}
		};

		for (auto s : SIGMA) {
			mix_g(v, 0, 4, 8, 12, input[s[0]], input[s[1]]);
			mix_g(v, 1, 5, 9, 13, input[s[2]], input[s[3]]);
			mix_g(v, 2, 6, 10, 14, input[s[4]], input[s[5]]);
			mix_g(v, 3, 7, 11, 15, input[s[6]], input[s[7]]);

			mix_g(v, 0, 5, 10, 15, input[s[8]], input[s[9]]);
			mix_g(v, 1, 6, 11, 12, input[s[10]], input[s[11]]);
			mix_g(v, 2, 7, 8, 13, input[s[12]], input[s[13]]);
			mix_g(v, 3, 4, 9, 14, input[s[14]], input[s[15]]);
		}

		for (int i = 0; i < 8; ++i) {
			state[i] ^= v[i] ^ v[8 + i];
		}
	}

	static constexpr void mix_g(u64 (&v)[16], u64 a, u64 b, u64 c, u64 d, u64 x, u64 y) {
		v[a] += v[b] + x;
		v[d] = rotate_right(v[d] ^ v[a], 32);
		v[c] += v[d];
		v[b] = rotate_right(v[b] ^ v[c], 24);
		v[a] += v[b] + y;
		v[d] = rotate_right(v[d] ^ v[a], 16);
		v[c] += v[d];
		v[b] = rotate_right(v[b] ^ v[c], 63);
	}
};

static void verify_fail() {}

constexpr int verify() {
	u32 a = 0x11111111;
	u32 b = 0x01020304;
	u32 c = 0x9B8D6F43;
	u32 d = 0x01234567;

	chacha20_quarter_round(a, b, c, d);

	if (a != 0xEA2A92F4) {
		verify_fail();
	}
	if (b != 0xCB1CF8CE) {
		verify_fail();
	}
	if (c != 0x4581472E) {
		verify_fail();
	}
	if (d != 0x5881C4BB) {
		verify_fail();
	}

	ChaCha20State state {
		0x879531E0, 0xC5ECF37D, 0x516461B1, 0xC9A62F8A,
		0x44C20EF3, 0x3390AF7F, 0xD9FC690B, 0x2A5F714C,
		0x53372767, 0xB00A5631, 0x974C541A, 0x359E9963,
		0x5C971061, 0x3D631689, 0x2098D9D6, 0x91DBD320
	};
	state.quarter_round(2, 7, 8, 13);
	if (state.state[0] != 0x879531E0) {
		verify_fail();
	}
	if (state.state[1] != 0xC5ECF37D) {
		verify_fail();
	}
	if (state.state[2] != 0xBDB886DC) {
		verify_fail();
	}
	if (state.state[3] != 0xC9A62F8A) {
		verify_fail();
	}
	if (state.state[4] != 0x44C20EF3) {
		verify_fail();
	}
	if (state.state[5] != 0x3390AF7F) {
		verify_fail();
	}
	if (state.state[6] != 0xD9FC690B) {
		verify_fail();
	}
	if (state.state[7] != 0xCFACAFD2) {
		verify_fail();
	}
	if (state.state[8] != 0xE46BEA80) {
		verify_fail();
	}
	if (state.state[9] != 0xB00A5631) {
		verify_fail();
	}
	if (state.state[10] != 0x974C541A) {
		verify_fail();
	}
	if (state.state[11] != 0x359E9963) {
		verify_fail();
	}
	if (state.state[12] != 0x5C971061) {
		verify_fail();
	}
	if (state.state[13] != 0xCCC07C79) {
		verify_fail();
	}
	if (state.state[14] != 0x2098D9D6) {
		verify_fail();
	}
	if (state.state[15] != 0x91DBD320) {
		verify_fail();
	}

	u32 block_key[8] {
		0x03020100, 0x07060504, 0x0B0A0908, 0x0F0E0D0C,
		0x13121110, 0x17161514, 0x1B1A1918, 0x1F1E1D1C
	};
	u32 block_nonce[3] {
		0x09000000, 0x4A000000, 0x00000000
	};
	auto block_test = cha_cha20_block(block_key, 1, block_nonce);
	if (block_test.state[0] != 0xE4E7F110) {
		verify_fail();
	}
	if (block_test.state[1] != 0x15593BD1) {
		verify_fail();
	}
	if (block_test.state[2] != 0x1FDD0F50) {
		verify_fail();
	}
	if (block_test.state[3] != 0xC47120A3) {
		verify_fail();
	}
	if (block_test.state[4] != 0xC7F4D1C7) {
		verify_fail();
	}
	if (block_test.state[5] != 0x0368C033) {
		verify_fail();
	}
	if (block_test.state[6] != 0x9AAA2204) {
		verify_fail();
	}
	if (block_test.state[7] != 0x4E6CD4C3) {
		verify_fail();
	}
	if (block_test.state[8] != 0x466482D2) {
		verify_fail();
	}
	if (block_test.state[9] != 0x09AA9F07) {
		verify_fail();
	}
	if (block_test.state[10] != 0x05D7C214) {
		verify_fail();
	}
	if (block_test.state[11] != 0xA2028BD9) {
		verify_fail();
	}
	if (block_test.state[12] != 0xD19C12B5) {
		verify_fail();
	}
	if (block_test.state[13] != 0xB94E16DE) {
		verify_fail();
	}
	if (block_test.state[14] != 0xE883D0CB) {
		verify_fail();
	}
	if (block_test.state[15] != 0x4E3C50A2) {
		verify_fail();
	}

	Blake2bState blake2b {};
	blake2b.init(64, nullptr, 0);
	if (blake2b.state[0] != 0x6A09E667F2BDC948) {
		verify_fail();
	}
	if (blake2b.state[1] != 0xBB67AE8584CAA73B) {
		verify_fail();
	}
	if (blake2b.state[2] != 0x3C6EF372FE94F82B) {
		verify_fail();
	}
	if (blake2b.state[3] != 0xA54FF53A5F1D36F1) {
		verify_fail();
	}
	if (blake2b.state[4] != 0x510E527FADE682D1) {
		verify_fail();
	}
	if (blake2b.state[5] != 0x9B05688C2B3E6C1F) {
		verify_fail();
	}
	if (blake2b.state[6] != 0x1F83D9ABFB41BD6B) {
		verify_fail();
	}
	if (blake2b.state[7] != 0x5BE0CD19137E2179) {
		verify_fail();
	}

	u64 value = 'a' | 'b' << 8 | 'c' << 16;
	blake2b.add_data(&value, 1);
	u64 digest[8];
	blake2b.generate_digest(digest);
	if (digest[0] != 0x47B2769FCFAA99D4) {
		verify_fail();
	}
	if (digest[1] != 0x33481B4207A384E3) {
		verify_fail();
	}
	if (digest[2] != 0x6BE603A9F6CE35A) {
		verify_fail();
	}
	if (digest[3] != 0xDE3C4E7AB372B53) {
		verify_fail();
	}
	if (digest[4] != 0xD4BB9E1DC95F4186) {
		verify_fail();
	}
	if (digest[5] != 0xB68EBAE3F1A083D3) {
		verify_fail();
	}
	if (digest[6] != 0x55332A18E58BAE4F) {
		verify_fail();
	}
	if (digest[7] != 0x471BB9CDD6AC785A) {
		verify_fail();
	}

	return 0;
}

[[gnu::unused]] constexpr int VERIFIER = verify();

namespace {
	struct Pool {
		void add_entropy(const u64* ptr, usize words) {
			IrqGuard irq_guard {};
			auto guard = data.lock();
			for (usize i = 0; i < words; ++i) {
				guard->push(ptr[i]);
			}
			event.signal_all();
		}

		bool is_full() {
			IrqGuard irq_guard {};
			return data.lock()->size() >= 512;
		}

		void wait_for_entropy() {
			event.wait();
		}

		Spinlock<kstd::vector<u64>> data;
		Event event;
	};

	ManuallyDestroy<Pool> POOLS[20] {};

	struct Nonce {
		void use() {
			inner[0] += 1;
			if (inner[0] == 0) {
				inner[1] += 1;
				if (inner[1] == 0) {
					inner[2] += 1;
				}
			}
		}

		u32 inner[3];
	};

	Nonce NONCE {};
	Spinlock<u32> POOL_INDEX {};
}

void random_add_entropy(const u64* data, usize words) {
	IrqGuard irq_guard {};

	for (auto& pool : POOLS) {
		if (!pool->is_full()) {
			pool->add_entropy(data, words);
		}
	}
}

void random_generate(void* data, usize size) {
	Blake2bState blake2b {};
	blake2b.init(32, nullptr, 0);

	Pool* pool;

	{
		IrqGuard irq_guard {};
		auto index_guard = POOL_INDEX.lock();

		pool = &*POOLS[*index_guard / 10];

		++*index_guard;
		if (*index_guard == 10 * 20) {
			*index_guard = 0;
		}
	}

	usize needed_entropy = size < 32 ? 32 : 128;

	usize entropy = 0;
	while (true) {
		{
			IrqGuard irq_guard {};
			auto guard = pool->data.lock();
			if (!guard->is_empty()) {
				usize to_add = kstd::min(guard->size(), usize {needed_entropy / 8});

				blake2b.add_data(guard->data() + guard->size() - to_add, to_add);
				entropy += to_add * 8;

				guard->resize(guard->size() - to_add);
			}
		}

		if (entropy >= needed_entropy) {
			break;
		}

		pool->wait_for_entropy();
	}

	u64 digest[4];
	blake2b.generate_digest(digest);

	u32 key[8];
	memcpy(key, digest, 32);

	auto* ptr = static_cast<u8*>(data);

	u32 counter = 0;
	for (usize i = 0; i < size; i += 64) {
		usize to_copy = kstd::min(size - i, usize {64});
		auto state = cha_cha20_block(key, counter++, NONCE.inner);
		memcpy(ptr + i, state.state, to_copy);
	}

	NONCE.use();
}
