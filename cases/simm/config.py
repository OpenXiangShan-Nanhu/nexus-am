import os
import argparse
import numpy as np
import time

def save_as_c_array(mat:np.array, filename:str, var_name="arr", dtype="uint8_t"):
    rows, cols = mat.shape
    with open(filename, "w") as f:
        f.write("#include <stdint.h>\n")
        f.write(f"{dtype} {var_name}[{rows}][{cols}] = {{\n")
        for i, row in enumerate(mat):
            row_str = ""
            if(dtype == "uint8_t"): row_str = ", ".join(f"0x{x:02x}" for x in row)
            elif(dtype == "uint16_t"): row_str = ", ".join(f"0x{x:04x}" for x in row)
            elif(dtype == "uint32_t"): row_str = ", ".join(f"0x{x:08x}" for x in row)
            else: row_str = ", ".join(f"0x{x:016x}" for x in row)
            f.write(f"    {{{row_str}}}")
            f.write(",\n" if i < rows - 1 else "\n")
        f.write("};\n")

def save_c_header(M:int, N:int, cores:int, filename:str):
    os.makedirs(os.path.dirname(filename), exist_ok=True)
    with open(filename, "w") as f:
        f.write("#ifndef __LINKNAN_SIMM_H__\n")
        f.write("#define __LINKNAN_SIMM_H__\n\n")
        f.write(f"#define M {M}\n")
        f.write(f"#define N {N}\n")
        f.write(f"#define NUM_CORES {cores}\n\n")
        f.write("#endif //__LINKNAN_SIMM_H__")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('dim', type=str, nargs='*', help='matrix dimension')
    parser.add_argument('-c', '--cores', type=int, default=4, help='cores number')
    args = parser.parse_args()

    seed = time.time_ns() % 2**32
    np.random.seed(seed)
    
    where_am_i = os.path.abspath(os.path.dirname(__file__))
    cores = args.cores
    dim = int(args.dim[0])
    M = dim * cores
    N = dim
    mat_a = np.random.randint(0, 1 << 16, size=(M, N), dtype=np.uint16)
    mat_b = np.random.randint(0, 1 << 16, size=(N, M), dtype=np.uint16)
    mat_ref = np.matmul(mat_a.astype(np.uint64), mat_b.astype(np.uint64))

    header_path = os.path.join(where_am_i, "include", "__gen_simm.h")
    mat_a_path = os.path.join(where_am_i, "src", "__gen_mat_a.c")
    mat_b_path = os.path.join(where_am_i, "src", "__gen_mat_b.c")
    mat_ref_path = os.path.join(where_am_i, "src", "__gen_mat_ref.c")

    save_c_header(M, N, cores, header_path)
    save_as_c_array(mat_a, mat_a_path, "MAT_A", "uint16_t")
    save_as_c_array(mat_b, mat_b_path, "MAT_B", "uint16_t")
    save_as_c_array(mat_ref, mat_ref_path, "MAT_REF", "uint64_t")