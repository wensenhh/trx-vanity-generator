#include "../utils/crypto.h"
#include "../utils/rng.h"
#include "../host/opencl_manager.h"
#include "test_ecc_kernel.h"
#include <iostream>
#include <iomanip>
#include <cstring>
#include <vector>
#include <array>
#include <sstream>

using namespace trx;

// Known test vectors for secp256k1
// Private key: 0x0000000000000000000000000000000000000000000000000000000000000001
// Public key X: 0x79BE667EF9DCBBAC55A06295CE870B07D29BFCDB2DCE28D959F2815B16F81798
// Public key Y: 0x483ADA7726A3C4655DA4FBFC0E1108A8FD17B448A68554199C47D08FFB10D4B8

void print_hex(const char* label, const uint8_t* data, size_t len) {
    std::cout << label << ": ";
    for (size_t i = 0; i < len; ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)data[i];
    }
    std::cout << std::dec << std::endl;
}

std::string bytes_to_hex_local(const uint8_t* data, size_t len) {
    std::ostringstream oss;
    for (size_t i = 0; i < len; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)data[i];
    }
    return oss.str();
}

bool compare_bytes(const uint8_t* a, const uint8_t* b, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

// ============================================================================
// CPU ECC Test
// ============================================================================
bool test_cpu_ecc() {
    std::cout << "=== CPU ECC Test ===" << std::endl;
    
    Secp256k1 ecc;
    
    // Test vector 1: private key = 1
    std::array<uint8_t, PRIVATE_KEY_SIZE> priv_key_1;
    priv_key_1.fill(0);
    priv_key_1[31] = 1;
    
    auto pub_key_1 = ecc.generate_public_key(priv_key_1);
    print_hex("CPU PubKey (k=1)", pub_key_1.data(), PUBLIC_KEY_SIZE);
    
    // Expected public key for k=1 (generator point)
    uint8_t expected_pub[64] = {
        0x79, 0xBE, 0x66, 0x7E, 0xF9, 0xDC, 0xBB, 0xAC, 0x55, 0xA0, 0x62, 0x95, 0xCE, 0x87, 0x0B, 0x07,
        0x02, 0x9B, 0xFC, 0xDB, 0x2D, 0xCE, 0x28, 0xD9, 0x59, 0xF2, 0x81, 0x5B, 0x16, 0xF8, 0x17, 0x98,
        0x48, 0x3A, 0xDA, 0x77, 0x26, 0xA3, 0xC4, 0x65, 0x5D, 0xA4, 0xFB, 0xFC, 0x0E, 0x11, 0x08, 0xA8,
        0xFD, 0x17, 0xB4, 0x48, 0xA6, 0x85, 0x54, 0x19, 0x9C, 0x47, 0xD0, 0x8F, 0xFB, 0x10, 0xD4, 0xB8
    };
    
    bool match = compare_bytes(pub_key_1.data(), expected_pub, 64);
    std::cout << "CPU k=1 matches expected: " << (match ? "YES" : "NO") << std::endl;
    
    // Test vector 2: private key = 2 (2G)
    std::array<uint8_t, PRIVATE_KEY_SIZE> priv_key_2;
    priv_key_2.fill(0);
    priv_key_2[31] = 2;
    
    auto pub_key_2 = ecc.generate_public_key(priv_key_2);
    print_hex("CPU PubKey (k=2)", pub_key_2.data(), PUBLIC_KEY_SIZE);
    
    // Test vector 3: private key = 3 (3G = 2G + G)
    std::array<uint8_t, PRIVATE_KEY_SIZE> priv_key_3;
    priv_key_3.fill(0);
    priv_key_3[31] = 3;
    
    auto pub_key_3 = ecc.generate_public_key(priv_key_3);
    print_hex("CPU PubKey (k=3)", pub_key_3.data(), PUBLIC_KEY_SIZE);
    
    // Test vector 4: random seed -> private key -> public key -> address
    RNG rng;
    auto seed = rng.generate_seed();
    auto seed_bytes = rng.seed_to_bytes(seed);
    auto priv_key = rng.derive_private_key(seed_bytes);
    auto pub_key = ecc.generate_public_key(priv_key);
    auto addr_bytes = ecc.generate_address_bytes(pub_key);
    std::string addr = Base58::encode_address(addr_bytes);
    
    std::cout << "CPU Random Seed: " << seed[0] << " " << seed[1] << " " << seed[2] << " " << seed[3] << std::endl;
    print_hex("CPU Private Key", priv_key.data(), PRIVATE_KEY_SIZE);
    print_hex("CPU Public Key", pub_key.data(), PUBLIC_KEY_SIZE);
    print_hex("CPU Address Bytes", addr_bytes.data(), TRX_ADDRESS_SIZE);
    std::cout << "CPU Address (Base58): " << addr << std::endl;
    
    return match;
}

// ============================================================================
// Focused GPU field operation test
// ============================================================================
bool test_gpu_field_ops() {
    std::cout << "\n=== GPU Field Ops Debug Test ===" << std::endl;
    static const char* names[14] = {
        "Gx2", "Gy2", "Y4", "X_Y2", "S", "M", "X3", "Y3", "Z3", "Zi", "Zi2", "Zi3", "Ax", "Ay"
    };
    static const char* expected[14] = {
        "8550e7d238fcf3086ba9adcf0fb52a9de3652194d06cb5bb38d50229b854fc49",
        "4866d6a5ab41ab2c6bcc57ccd3735da5f16f80a548e5e20a44e4e9b8118c26f2",
        "b3f3b2bfaada891a60cca114ac98c39d6ede11e439ee42ea773d88a81cb21c86",
        "72f065439bfb11453409c71af2e5920f5af2c2ef3c51f044b481d9cd5a73f0a8",
        "cbc1950e6fec4514d0271c6bcb96483d6bcb0bbcf147c112d207673669cfc671",
        "8ff2b776aaf6d91942fd096d2f1f7fd9aa2f64be71462131aa7f067e28fef8ac",
        "7d152c041ea8e1dc2191843d1fa9db55b68f88fef695e2c791d40444b365afc2",
        "56915849f52cc8f76f5fd7e4bf60db4a43bf633e1b1383f85fe89164bfadcbdb",
        "9075b4ee4d4788cabb49f7f81c221151fa2f68914d0aa833388fa11ff621a970",
        "b7e31a064ed74d314de79011c5f0a46ac155602353dc3d340fbeaeec9767a6a6",
        "66047bcf543e76c843bc725926c4e17022ebe0510e023cbe858b0c58506897a1",
        "6aeb6900990c34c4c49e44c1ce7b26c919eb0a46242082af9bc46c395cf0ac78",
        "c6047f9441ed7d6d3045406e95c07cd85c778e4b8cef3ca7abac09b95c709ee5",
        "1ae168fea63dc339a3c58419466ceaeef7f632653266d0e1236431a950cfe52a"
    };

    try {
        OpenCLManager cl;
        cl.initialize();
        cl.load_kernel_from_source("debug_field_ops_kernel", TEST_ECC_KERNEL_SOURCE);
        cl.build_program("-cl-std=CL1.2 -Werror -D__APPLE__=1");
        auto kernel = cl.get_kernel("debug_field_ops_kernel");
        constexpr size_t OUT_SIZE = 14 * 32;
        auto out_buf = cl.create_buffer(CL_MEM_WRITE_ONLY, OUT_SIZE);
        cl.set_kernel_arg_buffer(kernel, 0, out_buf);
        size_t global_size = 1;
        size_t local_size = 1;
        cl.enqueue_nd_range(kernel, 1, &global_size, &local_size);
        cl.finish();
        uint8_t out[OUT_SIZE];
        cl.read_buffer(out_buf, OUT_SIZE, out, true);

        bool all_ok = true;
        for (size_t i = 0; i < 14; ++i) {
            std::string actual = bytes_to_hex_local(out + i * 32, 32);
            bool ok = (actual == expected[i]);
            std::cout << names[i] << ": " << (ok ? "OK" : "FAIL") << std::endl;
            if (!ok) {
                std::cout << "  expected: " << expected[i] << std::endl;
                std::cout << "  actual:   " << actual << std::endl;
                all_ok = false;
                break;
            }
        }
        return all_ok;
    } catch (const std::exception& e) {
        std::cerr << "GPU field-op debug error: " << e.what() << std::endl;
        return false;
    }
}

// ============================================================================
// GPU ECC Test (OpenCL)
// ============================================================================
bool test_gpu_ecc() {
    std::cout << "\n=== GPU ECC Test ===" << std::endl;
    
    try {
        OpenCLManager cl;
        cl.initialize();
        
        auto device = cl.get_selected_device();
        std::cout << "GPU Device: " << device.name << std::endl;
        
        // Load test kernel from embedded source to bypass Apple OpenCL caching
        cl.load_kernel_from_source("test_ecc_kernel_v2", TEST_ECC_KERNEL_SOURCE);
        cl.build_program("-cl-std=CL1.2 -Werror -D__APPLE__=1");
        
        auto kernel = cl.get_kernel("test_ecc_kernel_v2");
        
        // Create buffers
        size_t priv_key_size = PRIVATE_KEY_SIZE * sizeof(cl_uchar);
        size_t pub_key_size = PUBLIC_KEY_SIZE * sizeof(cl_uchar);
        size_t addr_size = TRX_ADDRESS_SIZE * sizeof(cl_uchar);
        
        auto priv_buf = cl.create_buffer(CL_MEM_READ_ONLY, priv_key_size);
        auto pub_buf = cl.create_buffer(CL_MEM_WRITE_ONLY, pub_key_size);
        auto addr_buf = cl.create_buffer(CL_MEM_WRITE_ONLY, addr_size);
        
        cl.set_kernel_arg_buffer(kernel, 0, priv_buf);
        cl.set_kernel_arg_buffer(kernel, 1, pub_buf);
        cl.set_kernel_arg_buffer(kernel, 2, addr_buf);
        
        // Test vector 1: private key = 1
        cl_uchar priv_key_1[PRIVATE_KEY_SIZE] = {0};
        priv_key_1[31] = 1;
        
        cl.write_buffer(priv_buf, priv_key_size, priv_key_1, true);
        
        size_t global_size = 1;
        size_t local_size = 1;
        cl.enqueue_nd_range(kernel, 1, &global_size, &local_size);
        cl.finish();
        
        cl_uchar gpu_pub_key[PUBLIC_KEY_SIZE];
        cl_uchar gpu_addr[TRX_ADDRESS_SIZE];
        cl.read_buffer(pub_buf, pub_key_size, gpu_pub_key, true);
        cl.read_buffer(addr_buf, addr_size, gpu_addr, true);
        
        print_hex("GPU PubKey (k=1)", gpu_pub_key, PUBLIC_KEY_SIZE);
        print_hex("GPU Address (k=1)", gpu_addr, TRX_ADDRESS_SIZE);
        
        // Compare with CPU
        Secp256k1 ecc;
        std::array<uint8_t, PRIVATE_KEY_SIZE> cpu_priv;
        memcpy(cpu_priv.data(), priv_key_1, PRIVATE_KEY_SIZE);
        auto cpu_pub = ecc.generate_public_key(cpu_priv);
        auto cpu_addr = ecc.generate_address_bytes(cpu_pub);
        
        bool pub_match = compare_bytes(gpu_pub_key, cpu_pub.data(), PUBLIC_KEY_SIZE);
        bool addr_match = compare_bytes(gpu_addr, cpu_addr.data(), TRX_ADDRESS_SIZE);
        
        std::cout << "GPU/CPU PubKey match (k=1): " << (pub_match ? "YES" : "NO") << std::endl;
        std::cout << "GPU/CPU Address match (k=1): " << (addr_match ? "YES" : "NO") << std::endl;
        
        if (!pub_match) {
            print_hex("CPU PubKey", cpu_pub.data(), PUBLIC_KEY_SIZE);
            print_hex("GPU PubKey", gpu_pub_key, PUBLIC_KEY_SIZE);
            
            // Find first mismatch
            for (size_t i = 0; i < PUBLIC_KEY_SIZE; ++i) {
                if (cpu_pub.data()[i] != gpu_pub_key[i]) {
                    std::cout << "First mismatch at byte " << i << ": CPU=" << std::hex << (int)cpu_pub.data()[i] 
                              << " GPU=" << (int)gpu_pub_key[i] << std::dec << std::endl;
                    break;
                }
            }
        }
        
        // Test vector 2: private key = 2
        cl_uchar priv_key_2[PRIVATE_KEY_SIZE] = {0};
        priv_key_2[31] = 2;
        
        cl.write_buffer(priv_buf, priv_key_size, priv_key_2, true);
        cl.enqueue_nd_range(kernel, 1, &global_size, &local_size);
        cl.finish();
        
        cl.read_buffer(pub_buf, pub_key_size, gpu_pub_key, true);
        cl.read_buffer(addr_buf, addr_size, gpu_addr, true);
        
        print_hex("GPU PubKey (k=2)", gpu_pub_key, PUBLIC_KEY_SIZE);
        
        std::array<uint8_t, PRIVATE_KEY_SIZE> cpu_priv2;
        memcpy(cpu_priv2.data(), priv_key_2, PRIVATE_KEY_SIZE);
        auto cpu_pub2 = ecc.generate_public_key(cpu_priv2);
        print_hex("CPU PubKey (k=2)", cpu_pub2.data(), PUBLIC_KEY_SIZE);
        
        bool pub_match2 = compare_bytes(gpu_pub_key, cpu_pub2.data(), PUBLIC_KEY_SIZE);
        std::cout << "GPU/CPU PubKey match (k=2): " << (pub_match2 ? "YES" : "NO") << std::endl;
        
        // Test vector 3: private key = 3
        cl_uchar priv_key_3[PRIVATE_KEY_SIZE] = {0};
        priv_key_3[31] = 3;
        
        cl.write_buffer(priv_buf, priv_key_size, priv_key_3, true);
        cl.enqueue_nd_range(kernel, 1, &global_size, &local_size);
        cl.finish();
        
        cl.read_buffer(pub_buf, pub_key_size, gpu_pub_key, true);
        
        print_hex("GPU PubKey (k=3)", gpu_pub_key, PUBLIC_KEY_SIZE);
        
        std::array<uint8_t, PRIVATE_KEY_SIZE> cpu_priv3;
        memcpy(cpu_priv3.data(), priv_key_3, PRIVATE_KEY_SIZE);
        auto cpu_pub3 = ecc.generate_public_key(cpu_priv3);
        print_hex("CPU PubKey (k=3)", cpu_pub3.data(), PUBLIC_KEY_SIZE);
        
        bool pub_match3 = compare_bytes(gpu_pub_key, cpu_pub3.data(), PUBLIC_KEY_SIZE);
        std::cout << "GPU/CPU PubKey match (k=3): " << (pub_match3 ? "YES" : "NO") << std::endl;
        
        // Test vector 4: random private key
        RNG rng;
        auto seed = rng.generate_seed();
        auto seed_bytes = rng.seed_to_bytes(seed);
        auto priv_key = rng.derive_private_key(seed_bytes);
        
        cl.write_buffer(priv_buf, priv_key_size, priv_key.data(), true);
        cl.enqueue_nd_range(kernel, 1, &global_size, &local_size);
        cl.finish();
        
        cl.read_buffer(pub_buf, pub_key_size, gpu_pub_key, true);
        cl.read_buffer(addr_buf, addr_size, gpu_addr, true);
        
        auto cpu_pub_rand = ecc.generate_public_key(priv_key);
        auto cpu_addr_rand = ecc.generate_address_bytes(cpu_pub_rand);
        
        bool pub_match_rand = compare_bytes(gpu_pub_key, cpu_pub_rand.data(), PUBLIC_KEY_SIZE);
        bool addr_match_rand = compare_bytes(gpu_addr, cpu_addr_rand.data(), TRX_ADDRESS_SIZE);
        
        std::cout << "\nRandom Seed: " << seed[0] << " " << seed[1] << " " << seed[2] << " " << seed[3] << std::endl;
        print_hex("Random Private Key", priv_key.data(), PRIVATE_KEY_SIZE);
        std::cout << "GPU/CPU PubKey match (random): " << (pub_match_rand ? "YES" : "NO") << std::endl;
        std::cout << "GPU/CPU Address match (random): " << (addr_match_rand ? "YES" : "NO") << std::endl;
        
        if (!pub_match_rand) {
            print_hex("CPU PubKey", cpu_pub_rand.data(), PUBLIC_KEY_SIZE);
            print_hex("GPU PubKey", gpu_pub_key, PUBLIC_KEY_SIZE);
            
            for (size_t i = 0; i < PUBLIC_KEY_SIZE; ++i) {
                if (cpu_pub_rand.data()[i] != gpu_pub_key[i]) {
                    std::cout << "First mismatch at byte " << i << ": CPU=" << std::hex << (int)cpu_pub_rand.data()[i] 
                              << " GPU=" << (int)gpu_pub_key[i] << std::dec << std::endl;
                    break;
                }
            }
        }
        
        return pub_match && addr_match && pub_match2 && pub_match3 && pub_match_rand && addr_match_rand;
        
    } catch (const std::exception& e) {
        std::cerr << "GPU test error: " << e.what() << std::endl;
        return false;
    }
}

// ============================================================================
// Main
// ============================================================================
int main() {
    std::cout << "TRX Vanity Address - ECC Correctness Test" << std::endl;
    std::cout << "=========================================" << std::endl;
    
    bool cpu_ok = test_cpu_ecc();
    bool field_ok = test_gpu_field_ops();
    bool gpu_ok = test_gpu_ecc();
    
    std::cout << "\n=========================================" << std::endl;
    std::cout << "CPU Test: " << (cpu_ok ? "PASS" : "FAIL") << std::endl;
    std::cout << "GPU Field Ops: " << (field_ok ? "PASS" : "FAIL") << std::endl;
    std::cout << "GPU Test: " << (gpu_ok ? "PASS" : "FAIL") << std::endl;
    
    return (cpu_ok && field_ok && gpu_ok) ? 0 : 1;
}
