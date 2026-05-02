#include "../utils/crypto.h"
#include "../utils/rng.h"
#include "../host/opencl_manager.h"
#include <iostream>
#include <iomanip>
#include <cstring>
#include <vector>
#include <array>

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
        0xD2, 0x9B, 0xFC, 0xDB, 0x2D, 0xCE, 0x28, 0xD9, 0x59, 0xF2, 0x81, 0x5B, 0x16, 0xF8, 0x17, 0x98,
        0x48, 0x3A, 0xDA, 0x77, 0x26, 0xA3, 0xC4, 0x65, 0x5D, 0xA4, 0xFB, 0xFC, 0x0E, 0x11, 0x08, 0xA8,
        0xFD, 0x17, 0xB4, 0x48, 0xA6, 0x85, 0x54, 0x19, 0x99, 0xC4, 0x7D, 0x08, 0xFF, 0xB1, 0x0D, 0x4B
    };
    
    bool match = compare_bytes(pub_key_1.data(), expected_pub, 64);
    std::cout << "CPU k=1 matches expected: " << (match ? "YES" : "NO") << std::endl;
    
    // Test vector 2: random seed -> private key -> public key -> address
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
// GPU ECC Test (OpenCL)
// ============================================================================
bool test_gpu_ecc() {
    std::cout << "\n=== GPU ECC Test ===" << std::endl;
    
    try {
        OpenCLManager cl;
        cl.initialize();
        
        auto device = cl.get_selected_device();
        std::cout << "GPU Device: " << device.name << std::endl;
        
        // Load test kernel
        cl.load_kernel("test_ecc_kernel", "../kernel/test_ecc.cl");
        cl.build_program("-cl-std=CL1.2 -Werror");
        
        auto kernel = cl.get_kernel("test_ecc_kernel");
        
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
        
        // Test vector 2: random private key
        RNG rng;
        auto seed = rng.generate_seed();
        auto seed_bytes = rng.seed_to_bytes(seed);
        auto priv_key = rng.derive_private_key(seed_bytes);
        
        cl.write_buffer(priv_buf, priv_key_size, priv_key.data(), true);
        cl.enqueue_nd_range(kernel, 1, &global_size, &local_size);
        cl.finish();
        
        cl.read_buffer(pub_buf, pub_key_size, gpu_pub_key, true);
        cl.read_buffer(addr_buf, addr_size, gpu_addr, true);
        
        auto cpu_pub2 = ecc.generate_public_key(priv_key);
        auto cpu_addr2 = ecc.generate_address_bytes(cpu_pub2);
        
        bool pub_match2 = compare_bytes(gpu_pub_key, cpu_pub2.data(), PUBLIC_KEY_SIZE);
        bool addr_match2 = compare_bytes(gpu_addr, cpu_addr2.data(), TRX_ADDRESS_SIZE);
        
        std::cout << "\nRandom Seed: " << seed[0] << " " << seed[1] << " " << seed[2] << " " << seed[3] << std::endl;
        print_hex("Random Private Key", priv_key.data(), PRIVATE_KEY_SIZE);
        std::cout << "GPU/CPU PubKey match (random): " << (pub_match2 ? "YES" : "NO") << std::endl;
        std::cout << "GPU/CPU Address match (random): " << (addr_match2 ? "YES" : "NO") << std::endl;
        
        if (!pub_match2) {
            print_hex("CPU PubKey", cpu_pub2.data(), PUBLIC_KEY_SIZE);
            print_hex("GPU PubKey", gpu_pub_key, PUBLIC_KEY_SIZE);
            
            for (size_t i = 0; i < PUBLIC_KEY_SIZE; ++i) {
                if (cpu_pub2.data()[i] != gpu_pub_key[i]) {
                    std::cout << "First mismatch at byte " << i << ": CPU=" << std::hex << (int)cpu_pub2.data()[i] 
                              << " GPU=" << (int)gpu_pub_key[i] << std::dec << std::endl;
                    break;
                }
            }
        }
        
        return pub_match && addr_match && pub_match2 && addr_match2;
        
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
    bool gpu_ok = test_gpu_ecc();
    
    std::cout << "\n=========================================" << std::endl;
    std::cout << "CPU Test: " << (cpu_ok ? "PASS" : "FAIL") << std::endl;
    std::cout << "GPU Test: " << (gpu_ok ? "PASS" : "FAIL") << std::endl;
    
    return (cpu_ok && gpu_ok) ? 0 : 1;
}
