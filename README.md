# Quartz Programming Language v1.0.0

```
 ██████╗ ██╗   ██╗ █████╗ ██████╗ ████████╗███████╗
██╔═══██╗██║   ██║██╔══██╗██╔══██╗╚══██╔══╝╚════██║
██║   ██║██║   ██║███████║██████╔╝   ██║       ██╔╝
██║▄▄ ██║██║   ██║██╔══██║██╔══██╗   ██║      ██╔╝
╚██████╔╝╚██████╔╝██║  ██║██║  ██║   ██║      ██║
 ╚══▀▀═╝  ╚═════╝ ╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝      ╚═╝
```

**Low-level compiled language** didesain untuk logika akurat, komputasi cepat,
machine learning, AI, simulasi, dan game dengan logika kompleks.

| Property | Value |
|---|---|
| **Version** | 1.0.0 |
| **Architecture** | ARM64 (v8a/v9a) + x86_64 |
| **OS** | Linux |
| **Extension** | `.qua` |
| **Compiler** | C++ + inline ASM |
| **Output** | Native ELF binary |
| **Backend** | GAS (ARM64) / NASM (x86_64) |

---

## Daftar Isi

1. [Build & Install](#1-build--install)
2. [Quick Start](#2-quick-start)
3. [Struktur Program](#3-struktur-program)
4. [Commands CLI](#4-commands-cli)
5. [Syntax & Keywords Lengkap](#5-syntax--keywords-lengkap)
   - 5.1 [Core Keywords](#51-core-keywords)
   - 5.2 [Math / Logic (56 keywords)](#52-math--logic-56-keywords)
   - 5.3 [String (45 keywords)](#53-string-45-keywords)
   - 5.4 [CPU Control (34 keywords)](#54-cpu-control-34-keywords)
   - 5.5 [Hardware (59 keywords)](#55-hardware-59-keywords)
   - 5.6 [Data Structures (33 keywords)](#56-data-structures-33-keywords)
   - 5.7 [Text Manipulation (66 keywords)](#57-text-manipulation-66-keywords)
   - 5.8 [File & Folder (77 keywords)](#58-file--folder-77-keywords)
   - 5.9 [ASM Inline (12 keywords)](#59-asm-inline-12-keywords)
   - 5.10 [GPU Compute (54 keywords)](#510-gpu-compute-54-keywords)
   - 5.11 [GPU Render (67 keywords)](#511-gpu-render-67-keywords)
   - 5.12 [Memory RAM (45 keywords)](#512-memory-ram-45-keywords)
6. [Built-in Libraries](#6-built-in-libraries)
7. [Cara Belajar Quartz](#7-cara-belajar-quartz)
8. [Contoh Program Lengkap](#8-contoh-program-lengkap)
9. [Arsitektur Compiler](#9-arsitektur-compiler)

---

## 1. Build & Install

### Requirements
```bash
# Recian / Termux / Ubuntu ARM64
pkg install clang make binutils   # Recian/Termux
apt install g++ make binutils     # Ubuntu/Debian
```

### Build
```bash
unzip quartz_v1.0.0.zip
cd quartz
make
./quartz version
```

### Install system-wide
```bash
sudo make install
quartz --help
```

---

## 2. Quick Start

Buat file `hello.qua`:
```quartz
int main():

    print("Hello, Quartz!")
```

Jalankan:
```bash
./quartz run hello.qua
```

Compile ke binary:
```bash
./quartz compile hello.qua
./hello
```

---

## 3. Struktur Program

Setiap program Quartz **harus** dimulai dengan `int main():` dan setiap fungsi
diakhiri dengan `end!`.

```quartz
// Komentar satu baris

int main():

    // kode di sini
    print("hello")
```

### Indentasi
Quartz pakai **4 spasi** (atau tab). Block didefinisikan oleh `:` di akhir baris.

---

## 4. Commands CLI

| Command | Keterangan |
|---|---|
| `quartz run file.qua` | Compile + langsung jalankan |
| `quartz compile file.qua` | Compile ke binary |
| `quartz compile -o out file.qua` | Compile dengan nama output custom |
| `quartz compile --emit-asm file.qua` | Compile + simpan file `.asm` |
| `quartz compile --static file.qua` | Static linking |
| `quartz emit-asm file.qua` | Print generated assembly ke stdout |
| `quartz check file.qua` | Syntax check saja (tanpa compile) |
| `quartz tokens file.qua` | Dump token stream |
| `quartz ast file.qua` | Dump AST (Abstract Syntax Tree) |
| `quartz bench file.qua` | Benchmark compile time |
| `quartz test file.qua` | Compile + run sebagai test |
| `quartz fmt file.qua` | Format source code |
| `quartz new myproject` | Buat project baru |
| `quartz init` | Init project di folder sekarang |
| `quartz build` | Build dari `quartz.toml` |
| `quartz clean` | Hapus build artifacts |
| `quartz install lib"nama"` | Install package |
| `quartz uninstall lib"nama"` | Uninstall package |
| `quartz list-libs` | Lihat library yang terinstall |
| `quartz update` | Update toolchain |
| `quartz version` | Print versi |
| `quartz arch` | Info arsitektur CPU |
| `quartz info` | Info lengkap sistem |
| `quartz help` | Bantuan |

---

## 5. Syntax & Keywords Lengkap

### 5.1 Core Keywords

#### `int main():`
Entry point wajib di setiap program.
```quartz
int main():
    print("mulai program")
```

#### `print()`
Print nilai ke stdout dengan newline.
```quartz
int main():
    print("hello")
    var x = 42
    print(x)
    print(true)
```

#### `var`
Deklarasi variabel.
```quartz
var nama    = "quartz"
var angka   = 100
var pi      = 3.14
var flag    = true
var nothing = Nan
```

#### `input`
Ambil input dari user.
```quartz
var ketik = input("Masukkan nama: ")
print(ketik)
```

#### `func` ... `end!`
Deklarasi fungsi.
```quartz
func sapa(nama):
    print("Halo")
    print(nama)
end!

int main():
    sapa("Dunia")
```

Fungsi dengan return value:
```quartz
func tambah(a, b):
    return a + b
end!

int main():
    var hasil = tambah(3, 7)
    print(hasil)
```

#### `if` / `else` / `end!`
Percabangan.
```quartz
var x = 10

if x > 5:
    print("besar")
else:
    print("kecil")
end!
```

Nested if:
```quartz
if x > 0:
    if x > 100:
        print("sangat besar")
    else:
        print("positif")
    end!
end!
```

#### `when`
Alias untuk `if` (conditional block).
```quartz
when x == 42:
    print("empat dua")
end!
```

#### `from`
Loop dari nilai tertentu.
```quartz
from 1:
    print("mulai dari 1")
end!
```

#### `return`
Return nilai dari fungsi.
```quartz
func kuadrat(n):
    return n * n
end!
```

#### `util::`
Eksekusi ekspresi matematika langsung ke CPU.
```quartz
util::100 + 200
util::3 * 3 * 3
```

#### `MOV`
Move nilai ke variabel (assembly-style).
```quartz
var x = 0
MOV x 99
print(x)   // 99
```

#### `add`
Tambah nilai ke variabel.
```quartz
var n = 10
add n 5
print(n)   // 15
```

#### `save::`
Simpan nilai dengan key.
```quartz
var source = 42
save::dest = key = source
```

#### `split()`
Pecah variabel berdasarkan delimiter koma.
```quartz
var csv = "a,b,c"
split(csv)
```

#### `point`
Akses element dari variabel yang sudah di-split.
```quartz
var nums = 1
point nums = 1
```

#### `wait`
Tunggu N milidetik.
```quartz
wait 1000    // tunggu 1 detik
```

#### `reverse`
Reverse state/operasi.
```quartz
reverse
```

#### `import`
Import fungsi dari file `.qua` atau library.
```quartz
import "quartz.math"
import "mylib.qua"
```

#### `using()`
Gunakan file `.qua` lain.
```quartz
using("utils.qua")
```

#### `GPURender::`
Trigger GPU render operation.
```quartz
GPURender::gr_draw(vertices, 3)
GPURender::gr_present()
```

#### `true` / `false` / `Nan`
Boolean dan NaN literals.
```quartz
var t = true
var f = false
var n = Nan
```

#### `and` / `or` / `not`
Logical operators.
```quartz
if x > 0 and x < 100:
    print("dalam range")
end!

if not flag:
    print("flag false")
end!
```

---

### 5.2 Math / Logic (56 keywords)

| Keyword | Keterangan | Contoh |
|---|---|---|
| `abs(x)` | Nilai absolut | `abs(-42)` → `42` |
| `sqrt(x)` | Akar kuadrat | `sqrt(16)` → `4` |
| `pow(b,e)` | Perpangkatan | `pow(2,10)` → `1024` |
| `log(x)` | Log natural | `log(2)` |
| `log2(x)` | Log basis 2 | `log2(8)` → `3` |
| `log10(x)` | Log basis 10 | `log10(100)` → `2` |
| `exp(x)` | e^x | `exp(1)` |
| `sin(x)` | Sinus (radian) | `sin(0)` → `0` |
| `cos(x)` | Cosinus | `cos(0)` → `1` |
| `tan(x)` | Tangen | `tan(0)` → `0` |
| `asin(x)` | Arc sinus | `asin(1)` |
| `acos(x)` | Arc cosinus | `acos(0)` |
| `atan(x)` | Arc tangen | `atan(1)` |
| `atan2(y,x)` | Arc tangen 2 argumen | `atan2(1,1)` |
| `ceil(x)` | Pembulatan ke atas | `ceil(3.2)` → `4` |
| `floor(x)` | Pembulatan ke bawah | `floor(3.9)` → `3` |
| `round(x)` | Pembulatan terdekat | `round(3.5)` → `4` |
| `min(a,b)` | Nilai minimum | `min(3,7)` → `3` |
| `max(a,b)` | Nilai maksimum | `max(3,7)` → `7` |
| `clamp(x,lo,hi)` | Batasi nilai | `clamp(150,0,100)` → `100` |
| `lerp(a,b,t)` | Linear interpolation | `lerp(0,10,0.5)` → `5` |
| `fmod(a,b)` | Float modulo | `fmod(10.5,3)` |
| `mod(a,b)` | Integer modulo | `mod(10,3)` → `1` |
| `dot(a,b)` | Dot product vektor | |
| `cross(a,b)` | Cross product | |
| `normalize(v)` | Normalisasi vektor | |
| `magnitude(v)` | Panjang vektor | |
| `distance(a,b)` | Jarak antar vektor | |
| `matmul(a,b)` | Matrix multiplication | |
| `matinv(m)` | Matrix inverse | |
| `mattrans(m)` | Matrix transpose | |
| `det(m)` | Determinant matrix | |
| `eigen(m)` | Eigenvalue | |
| `conv(in,k)` | Konvolusi 1D | |
| `fft(sig)` | Fast Fourier Transform | |
| `ifft(spec)` | Inverse FFT | |
| `sigmoid(x)` | Fungsi sigmoid ML | `sigmoid(0)` → `0.5` |
| `relu(x)` | ReLU activation | `relu(-1)` → `0` |
| `softmax(arr)` | Softmax | |
| `tanh_act(x)` | Tanh activation | |
| `derivative(f,x)` | Turunan numerik | |
| `integral(f,a,b)` | Integral numerik | |
| `gcd(a,b)` | FPB | `gcd(48,18)` → `6` |
| `lcm(a,b)` | KPK | `lcm(4,6)` → `12` |
| `isprime(n)` | Cek bilangan prima | `isprime(17)` → `1` |
| `factorial(n)` | Faktorial | `factorial(5)` → `120` |
| `fibonacci(n)` | Fibonacci ke-n | `fibonacci(10)` → `55` |
| `comb(n,k)` | Kombinasi C(n,k) | `comb(5,2)` → `10` |
| `perm(n,k)` | Permutasi P(n,k) | `perm(5,2)` → `20` |
| `median(arr)` | Nilai tengah | |
| `mean(arr)` | Rata-rata | |
| `variance(arr)` | Variansi | |
| `stddev(arr)` | Standar deviasi | |
| `covariance(a,b)` | Kovariansi | |
| `pearson(a,b)` | Korelasi Pearson | |
| `entropy(probs)` | Entropi informasi | |

Contoh:
```quartz
int main():
    var s = sqrt(144)
    print(s)              // 12

    var p = isprime(17)
    print(p)              // 1 (true)

    var f = factorial(6)
    print(f)              // 720

    var g = gcd(48, 18)
    print(g)              // 6

    var sig = sigmoid(0)
    print(sig)            // 0 (integer approx of 0.5)

    var r = relu(-5)
    print(r)              // 0

    var r2 = relu(3)
    print(r2)             // 3
```

---

### 5.3 String (45 keywords)

| Keyword | Keterangan |
|---|---|
| `strlen(s)` | Panjang string |
| `strupper(s)` | Uppercase |
| `strlower(s)` | Lowercase |
| `strtrim(s)` | Trim spasi kanan-kiri |
| `strltrim(s)` | Trim spasi kiri |
| `strrtrim(s)` | Trim spasi kanan |
| `strcat(a,b)` | Gabung string |
| `substr(s,start,len)` | Substring |
| `strfind(s,needle)` | Cari substring |
| `strrfind(s,needle)` | Cari dari kanan |
| `strreplace(s,old,new)` | Replace substring |
| `strsplit(s,delim)` | Split string |
| `strjoin(arr,delim)` | Join array ke string |
| `strstarts(s,prefix)` | Cek awalan |
| `strends(s,suffix)` | Cek akhiran |
| `strcontains(s,sub)` | Cek mengandung |
| `strcount(s,sub)` | Hitung kemunculan |
| `strrepeat(s,n)` | Ulangi string |
| `strrev(s)` | Balik string |
| `strpadl(s,n,c)` | Pad kiri |
| `strpadr(s,n,c)` | Pad kanan |
| `strformat(fmt,...)` | Format string |
| `strhash(s)` | Hash string |
| `strtoint(s)` | String ke integer |
| `strtofloat(s)` | String ke float |
| `strfromint(n)` | Integer ke string |
| `strfromfloat(f)` | Float ke string |
| `strcmp(a,b)` | Bandingkan string |
| `strregex(s,pat)` | Regex match |
| `strb64enc(s)` | Base64 encode |
| `strb64dec(s)` | Base64 decode |
| `strhex(s)` | String ke hex |
| `strfromhex(h)` | Hex ke string |
| `charat(s,i)` | Karakter ke-i |
| `strbytes(s)` | Byte array dari string |
| `strinsert(s,i,sub)` | Insert substring |
| `strdelete(s,i,n)` | Hapus substring |

---

### 5.4 CPU Control (34 keywords)

| Keyword | Keterangan |
|---|---|
| `cpu_pause()` | CPU pause/yield hint |
| `cpu_yield()` | Thread yield |
| `cpu_barrier()` | Full memory barrier |
| `cpu_fence()` | Memory fence |
| `cpu_mfence()` | Memory fence (x86) |
| `cpu_lfence()` | Load fence (x86) |
| `cpu_sfence()` | Store fence (x86) |
| `cpu_clflush(addr)` | Flush cache line |
| `cpu_prefetch(addr)` | Prefetch ke cache |
| `cpuid()` | CPUID instruction |
| `rdtsc()` | Read timestamp counter |
| `rdtscp()` | RDTSCP |
| `getcores()` | Jumlah CPU core |
| `getfreq()` | Frekuensi CPU |
| `setaff(mask)` | Set CPU affinity |
| `getaff()` | Get CPU affinity |
| `sleep_ns(ns)` | Sleep nanosecond |
| `cpu_spin(n)` | Spin loop |
| `atomic_load(ptr)` | Atomic load |
| `atomic_store(ptr,v)` | Atomic store |
| `cas(ptr,exp,des)` | Compare-and-swap |
| `fetchadd(ptr,v)` | Fetch-and-add |
| `xchg(ptr,v)` | Exchange |
| `cpu_lock()` | Acquire spinlock |
| `cpu_unlock()` | Release spinlock |
| `nop()` | No operation |
| `hlt()` | Halt CPU |
| `cpu_int(n)` | Software interrupt |
| `cli()` | Disable interrupts |
| `sti()` | Enable interrupts |
| `pushf()` | Push flags |
| `popf()` | Pop flags |
| `syscall()` | System call |
| `sysret()` | System return |

---

### 5.5 Hardware (59 keywords)

| Keyword | Keterangan |
|---|---|
| `hw_mmap(addr,size)` | Memory-mapped I/O |
| `hw_munmap(ptr,size)` | Unmap memory |
| `ioread8/16/32/64(port)` | I/O port read |
| `iowrite8/16/32/64(port,v)` | I/O port write |
| `dma_alloc(size)` | DMA allocate |
| `dma_free(ptr)` | DMA free |
| `dma_sync(ptr,size)` | DMA sync |
| `irq_reg(irq,handler)` | Register IRQ |
| `irq_en(irq)` | Enable IRQ |
| `irq_dis(irq)` | Disable IRQ |
| `timer_ns/us/ms()` | High-res timer |
| `port_in/out(port,v)` | Port I/O |
| `devopen/close(path)` | Device open/close |
| `devread/write(fd,buf,n)` | Device read/write |
| `devioctl(fd,cmd,arg)` | ioctl |
| `netopen/close(addr)` | Network socket |
| `netsend/recv(fd,buf,n)` | Network send/recv |
| `netbind/listen/accept(fd)` | Server socket |
| `netconn(fd,addr)` | Connect |
| `soundopen/close()` | Audio device |
| `soundplay/stop(buf)` | Play audio |
| `dispopen/close()` | Display device |
| `dispflush()` | Flush display |
| `kbdread()` | Keyboard input |
| `mouseread()` | Mouse input |
| `gamepad()` | Gamepad state |
| `camera()` | Camera capture |
| `gpio_set/get(pin,v)` | GPIO control |
| `gpio_mode(pin,mode)` | GPIO mode |
| `spi_xfer(data)` | SPI transfer |
| `i2c_xfer(addr,data)` | I2C transfer |
| `uart_send(port,data)` | UART send |

---

### 5.6 Data Structures (33 keywords)

| Keyword | Keterangan |
|---|---|
| `array` | Array declaration |
| `dict` | Dictionary |
| `set` | Set |
| `queue` | Queue |
| `stack` | Stack |
| `list` | Linked list |
| `tuple` | Tuple |
| `map` | Map/hashmap |
| `push(coll,v)` | Push element |
| `pop(coll)` | Pop element |
| `peek(coll)` | Peek top |
| `insert(coll,i,v)` | Insert at index |
| `remove(coll,i)` | Remove at index |
| `clear(coll)` | Clear collection |
| `size(coll)` | Size of collection |
| `empty(coll)` | Check empty |
| `front(coll)` | Front element |
| `back(coll)` | Back element |
| `sort(coll)` | Sort ascending |
| `rsort(coll)` | Sort descending |
| `filter(coll,fn)` | Filter elements |
| `reduce(coll,fn)` | Reduce |
| `mapfn(coll,fn)` | Map function |
| `foreach(coll,fn)` | For each |
| `findidx(coll,v)` | Find index |
| `slice(coll,a,b)` | Slice |
| `concatarr(a,b)` | Concat arrays |
| `unique(coll)` | Unique elements |
| `flatten(coll)` | Flatten nested |
| `zip(a,b)` | Zip two arrays |
| `enumerate(coll)` | Enumerate |
| `reversearr(coll)` | Reverse array |
| `containsval(coll,v)` | Contains value |

---

### 5.7 Text Manipulation (66 keywords)

| Keyword | Keterangan |
|---|---|
| `text_wrap(s,width)` | Word wrap |
| `text_center(s,width)` | Center align |
| `text_left/right(s,w)` | Align text |
| `text_bold/italic(s)` | ANSI bold/italic |
| `text_color(s,color)` | ANSI color |
| `text_under/strike(s)` | Underline/strikethrough |
| `text_reset()` | Reset ANSI |
| `text_clear()` | Clear terminal |
| `text_cursor(r,c)` | Move cursor |
| `text_box(s,w,h)` | Draw text box |
| `text_table(data)` | Render table |
| `printf(fmt,...)` | printf-style |
| `scanf(fmt,...)` | scanf-style |
| `sprintf(buf,fmt,...)` | sprintf |
| `snprintf(buf,n,fmt,...)` | snprintf |
| `puts(s)` | Print string+newline |
| `gets(buf)` | Read line |
| `putchar(c)` | Print char |
| `getchar()` | Read char |
| `atoi(s)` | String to int |
| `atof(s)` | String to float |
| `memcpy(d,s,n)` | Copy memory |
| `memmove(d,s,n)` | Move memory |
| `memset(p,v,n)` | Set memory |
| `memcmp(a,b,n)` | Compare memory |
| `printhex(n)` | Print hex |
| `printbin(n)` | Print binary |

---

### 5.8 File & Folder (77 keywords)

| Keyword | Keterangan |
|---|---|
| `file_open(path,mode)` | Buka file |
| `file_close(fd)` | Tutup file |
| `file_read(fd,buf,n)` | Baca file |
| `file_write(fd,data)` | Tulis file |
| `file_append(fd,data)` | Append ke file |
| `file_seek(fd,pos)` | Seek position |
| `file_tell(fd)` | Posisi sekarang |
| `file_size(path)` | Ukuran file |
| `file_exists(path)` | Cek file ada |
| `file_delete(path)` | Hapus file |
| `file_rename(old,new)` | Rename file |
| `file_copy(src,dst)` | Copy file |
| `file_move(src,dst)` | Move file |
| `file_chmod(path,mode)` | Ubah permission |
| `file_readall(path)` | Baca semua isi |
| `file_writeall(path,data)` | Tulis semua |
| `file_readline(fd)` | Baca satu baris |
| `file_writeln(fd,line)` | Tulis satu baris |
| `dir_create(path)` | Buat direktori |
| `dir_delete(path)` | Hapus direktori |
| `dir_exists(path)` | Cek direktori |
| `dir_list(path)` | List isi direktori |
| `cwd()` | Current directory |
| `chdir(path)` | Ganti direktori |
| `mkdir_p(path)` | mkdir -p |
| `glob(pattern)` | Glob pattern |
| `path_join(a,b)` | Join path |
| `basename(path)` | Nama file |
| `dirname(path)` | Direktori parent |
| `extname(path)` | Ekstensi file |
| `abspath(path)` | Absolute path |
| `fs_space(path)` | Total space |
| `fs_free(path)` | Free space |
| `proc_exec(cmd)` | Execute command |
| `proc_wait(pid)` | Wait process |
| `proc_kill(pid)` | Kill process |
| `getpid()` | PID sekarang |
| `env_get(key)` | Get env var |
| `env_set(key,val)` | Set env var |
| `symlink(src,dst)` | Buat symlink |
| `readlink(path)` | Baca symlink |

---

### 5.9 ASM Inline (12 keywords)

Langsung eksekusi instruksi assembly:

| Keyword | Keterangan |
|---|---|
| `asm_mov(dst,src)` | MOV instruction |
| `asm_add(dst,src)` | ADD instruction |
| `asm_sub(dst,src)` | SUB instruction |
| `asm_mul(dst,src)` | MUL instruction |
| `asm_div(dst,src)` | DIV instruction |
| `asm_and(dst,src)` | AND instruction |
| `asm_or(dst,src)` | OR instruction |
| `asm_xor(dst,src)` | XOR instruction |
| `asm_shl(dst,n)` | Shift left |
| `asm_shr(dst,n)` | Shift right |
| `asm_cmp(a,b)` | Compare |
| `asm_jmp(label)` | Jump |

---

### 5.10 GPU Compute (54 keywords)

| Keyword | Keterangan |
|---|---|
| `gpu_init()` | Init GPU |
| `gpu_shutdown()` | Shutdown GPU |
| `gpu_alloc(size)` | Alokasi GPU memory |
| `gpu_free(ptr)` | Free GPU memory |
| `gpu_upload(dst,src,n)` | Upload ke GPU |
| `gpu_download(dst,src,n)` | Download dari GPU |
| `gpu_sync()` | Sync GPU |
| `gpu_kernel(fn)` | Define kernel |
| `gpu_launch(fn,grid,block)` | Launch kernel |
| `gpu_matmul(a,b,out)` | Matrix multiply GPU |
| `gpu_conv2d(in,k,out)` | 2D Convolution GPU |
| `gpu_relu(in,out,n)` | ReLU GPU |
| `gpu_softmax(in,out,n)` | Softmax GPU |
| `batchnorm(x,m,s,g,b)` | Batch normalization |
| `dropout(x,rate)` | Dropout |
| `attention(q,k,v,dim)` | Attention mechanism |
| `gpu_fft(data,n)` | FFT on GPU |
| `gpu_reduce(arr,n)` | Reduction |
| `gpu_sort(arr,n)` | Sort on GPU |
| `warp_reduce(val)` | Warp reduction |
| `gpu_info()` | GPU info |
| `gpu_device(n)` | Select GPU device |

---

### 5.11 GPU Render (67 keywords)

| Keyword | Keterangan |
|---|---|
| `gr_pipeline(vert,frag)` | Setup render pipeline |
| `gr_vertex(data,n)` | Vertex buffer |
| `gr_fragment(shader)` | Fragment shader |
| `gr_texture(img,w,h)` | Upload texture |
| `gr_draw(vao,count)` | Draw call |
| `gr_drawidx(vao,ibo,n)` | Draw indexed |
| `gr_clear()` | Clear framebuffer |
| `gr_viewport(x,y,w,h)` | Set viewport |
| `gr_depth(enable)` | Depth testing |
| `gr_blend(src,dst)` | Alpha blending |
| `gr_cull(mode)` | Face culling |
| `gr_framebuf(w,h)` | Framebuffer |
| `gr_present()` | Present frame |
| `gr_swap()` | Swap buffers |
| `vsync(enable)` | VSync |
| `ssao()` | Ambient occlusion |
| `bloom(threshold)` | Bloom effect |
| `hdr(enable)` | HDR rendering |
| `tonemap(mode)` | Tonemapping |
| `gamma(value)` | Gamma correction |
| `shadows(enable)` | Shadow mapping |
| `skybox(cubemap)` | Skybox |
| `particles(n,cfg)` | Particle system |
| `bones(skeleton)` | Skeletal animation |
| `skin(mesh,bones)` | Skinning |
| `wireframe(enable)` | Wireframe mode |

---

### 5.12 Memory RAM (45 keywords)

| Keyword | Keterangan |
|---|---|
| `mem_alloc(var,size,align)` | Alokasi memory |
| `mem_free(ptr)` | Bebaskan memory |
| `mem_realloc(ptr,size)` | Realokasi |
| `mem_calloc(n,size)` | Calloc |
| `mem_zero(ptr,n)` | Zero memory |
| `mem_copy(dst,src,n)` | Copy memory |
| `mem_move(dst,src,n)` | Move memory |
| `mem_set(ptr,val,n)` | Set memory |
| `mem_cmp(a,b,n)` | Compare memory |
| `mem_pool()` | Memory pool |
| `mem_arena()` | Arena allocator |
| `mem_page(n)` | Page allocation |
| `mem_huge(n)` | Huge page |
| `mem_lock(ptr,n)` | Lock pages in RAM |
| `mem_unlock(ptr,n)` | Unlock pages |
| `mem_protect(ptr,n,flags)` | Memory protection |
| `mem_advise(ptr,n,advice)` | madvise |
| `mem_fence()` | Memory fence |
| `mem_barrier()` | Memory barrier |
| `mem_prefetch(ptr)` | Prefetch |
| `mem_cas(ptr,exp,des)` | Atomic CAS |
| `mem_fadd(ptr,val)` | Fetch-and-add |
| `mem_xchg(ptr,val)` | Exchange |
| `refcnt(ptr)` | Reference count |
| `weakref(ptr)` | Weak reference |
| `gc_mark(ptr)` | GC mark |
| `gc_sweep()` | GC sweep |
| `compact()` | Compact memory |
| `defrag()` | Defragment |

---

## 6. Built-in Libraries

Install dengan `quartz install lib"nama"`:

### `quartz.math`
```quartz
import "quartz.math"

int main():
    var r = qz_sqrt(16)
    var s = qz_sigmoid(1)
    var f = qz_fibonacci(10)
    print(r)
    print(f)
```

Fungsi tersedia: `qz_add`, `qz_sub`, `qz_mul`, `qz_div`, `qz_clamp`, `qz_lerp`,
`qz_sqrt`, `qz_pow`, `qz_sin`, `qz_cos`, `qz_tan`, `qz_abs`, `qz_ceil`,
`qz_floor`, `qz_round`, `qz_log`, `qz_exp`, `qz_sigmoid`, `qz_relu`,
`qz_tanh`, `qz_gcd`, `qz_lcm`, `qz_isprime`, `qz_factorial`, `qz_fibonacci`,
`qz_combination`, `qz_permutation`, `qz_mean`, `qz_stddev`, `qz_variance`,
`qz_pearson`, `qz_entropy`

### `quartz.io`
```quartz
import "quartz.io"

int main():
    qz_println("Hello")
    var x = qz_read_int()
    qz_print_int(x)
```

Fungsi: `qz_print`, `qz_println`, `qz_read_line`, `qz_read_int`,
`qz_read_float`, `qz_print_int`, `qz_print_float`, `qz_file_read_all`,
`qz_file_write_all`, `qz_file_exists`, `qz_dir_list`

### `quartz.ml`
```quartz
import "quartz.ml"

int main():
    var out = qz_sigmoid(2)
    var r = qz_relu(-3)
    var loss = qz_mse(out, 1)
    print(loss)
```

Fungsi: `qz_sigmoid`, `qz_relu`, `qz_leaky_relu`, `qz_elu`, `qz_tanh`,
`qz_swish`, `qz_softmax_1d`, `qz_mse`, `qz_mae`, `qz_cross_entropy`,
`qz_binary_crossentropy`, `qz_hinge_loss`, `qz_sgd_step`, `qz_momentum_step`,
`qz_batch_norm`, `qz_layer_norm`, `qz_dropout`, `qz_cosine_similarity`,
`qz_xavier_init`, `qz_he_init`, `qz_scaled_dot_attention`

---

## 7. Cara Belajar Quartz

### Level 1 — Dasar (Mulai di sini)

**Step 1: Hello World**
```quartz
int main():
    print("Hello, World!")
```
Jalankan: `quartz run hello.qua`

**Step 2: Variabel dan print**
```quartz
int main():
    var nama = "Alice"
    var umur = 20
    var tinggi = 165

    print(nama)
    print(umur)
    print(tinggi)
```

**Step 3: Operasi matematika**
```quartz
int main():
    var a = 10
    var b = 3

    var jumlah = a + b
    var selisih = a - b
    var kali = a * b
    var bagi = a / b

    print(jumlah)   // 13
    print(selisih)  // 7
    print(kali)     // 30
    print(bagi)     // 3
```

**Step 4: Percabangan**
```quartz
int main():
    var nilai = 85

    if nilai >= 90:
        print("A")
    else:
        if nilai >= 80:
            print("B")
        else:
            print("C")
        end!
    end!
```

**Step 5: Fungsi sederhana**
```quartz
func luas_persegi(sisi):
    return sisi * sisi
end!

func keliling_persegi(sisi):
    return sisi * 4
end!

int main():
    var s = 5
    var L = luas_persegi(s)
    var K = keliling_persegi(s)
    print(L)   // 25
    print(K)   // 20
```

---

### Level 2 — Menengah

**Step 6: Fungsi matematika bawaan**
```quartz
int main():
    // Akar kuadrat
    var r = sqrt(81)
    print(r)        // 9

    // Perpangkatan
    var p = pow(2, 8)
    print(p)        // 256

    // Nilai absolut
    var a = abs(-99)
    print(a)        // 99

    // Clamp nilai
    var c = clamp(150, 0, 100)
    print(c)        // 100

    // Min dan max
    print(min(3, 7))   // 3
    print(max(3, 7))   // 7
```

**Step 7: Number theory**
```quartz
int main():
    // FPB dan KPK
    var g = gcd(48, 18)
    print(g)   // 6

    var l = lcm(4, 6)
    print(l)   // 12

    // Bilangan prima
    print(isprime(2))    // 1
    print(isprime(17))   // 1
    print(isprime(18))   // 0

    // Faktorial
    var f = factorial(10)
    print(f)   // 3628800

    // Fibonacci
    var fib = fibonacci(10)
    print(fib)  // 55
```

**Step 8: Fungsi rekursif-style (iteratif)**
```quartz
func hitung_faktorial(n):
    var hasil = 1
    var i = n
    // Manual loop via recursive calls or use built-in
    return factorial(n)
end!

func cek_prima(n):
    return isprime(n)
end!

int main():
    var f5 = hitung_faktorial(5)
    print(f5)   // 120

    print(cek_prima(13))  // 1
    print(cek_prima(14))  // 0
```

**Step 9: util:: untuk komputasi cepat**
```quartz
int main():
    // Langsung ke CPU
    util::1000 * 1000
    util::255 + 1
    util::2 * 2 * 2 * 2 * 2
```

**Step 10: MOV dan ADD**
```quartz
int main():
    var score = 0

    // Langkah 1: set score
    MOV score 100

    // Tambah bonus
    add score 50

    print(score)  // 150
```

---

### Level 3 — Lanjutan

**Step 11: Machine Learning basics**
```quartz
int main():
    // Neuron sederhana
    var w1 = 1
    var w2 = 2
    var b = 1
    var x1 = 1
    var x2 = 0

    // Forward pass
    var z = w1 * x1 + w2 * x2 + b
    print(z)   // 2

    // Aktivasi sigmoid
    var a = sigmoid(z)
    print(a)

    // Aktivasi ReLU
    var r = relu(z)
    print(r)   // 2

    // ReLU dengan negatif
    var r2 = relu(-3)
    print(r2)  // 0

    // Loss MSE
    var target = 1
    var diff = a - target
    var loss = diff * diff
    print(loss)
```

**Step 12: Simulasi fisika**
```quartz
int main():
    // Kinematika sederhana
    var v0 = 20    // kecepatan awal (m/s)
    var g = 10     // gravitasi (m/s²)
    var t = 2      // waktu (detik)

    // Kecepatan setelah t detik
    var vt = v0 - g * t
    print(vt)   // 0

    // Jarak
    var d = v0 * t - g * t * t / 2
    print(d)    // 20

    // Jarak Pythagoras
    var dx = 3
    var dy = 4
    var jarak = sqrt(dx * dx + dy * dy)
    print(jarak)  // 5
```

**Step 13: Logika game**
```quartz
func hitung_damage(atk, def):
    var dmg = atk - def
    if dmg < 1:
        MOV dmg 1
    end!
    return dmg
end!

func cek_menang(hp):
    if hp <= 0:
        return 1
    else:
        return 0
    end!
end!

int main():
    print("=== Combat Simulator ===")

    var player_hp  = 100
    var enemy_hp   = 80
    var player_atk = 25
    var player_def = 10
    var enemy_atk  = 15

    // Round 1
    var dmg_ke_enemy  = hitung_damage(player_atk, player_def)
    var dmg_ke_player = hitung_damage(enemy_atk, player_def)

    var enemy_hp_baru  = enemy_hp  - dmg_ke_enemy
    var player_hp_baru = player_hp - dmg_ke_player

    print(dmg_ke_enemy)
    print(dmg_ke_player)
    print(enemy_hp_baru)
    print(player_hp_baru)

    if cek_menang(enemy_hp_baru):
        print("Player menang!")
    else:
        print("Battle lanjut...")
    end!
```

---

### Level 4 — Expert

**Step 14: Gunakan built-in library**
```quartz
import "quartz.math"
import "quartz.ml"

int main():
    // Neural net layer
    var in1 = 1
    var in2 = 0

    var w11 = 1
    var w12 = 2
    var w21 = 3
    var w22 = 4

    // Layer compute
    var z1 = w11 * in1 + w12 * in2
    var z2 = w21 * in1 + w22 * in2

    var a1 = qz_sigmoid(z1)
    var a2 = qz_relu(z2)

    print(a1)
    print(a2)
```

**Step 15: Project struktur dengan quartz.toml**
```bash
quartz new myapp
cd myapp
# Edit src/main.qua
quartz build
./bin/myapp
```

**Step 16: CPU-level programming**
```quartz
int main():
    // Memory barrier
    mem_fence()

    // CPU pause (reduce power dalam spin loop)
    cpu_pause()

    // Timestamp counter
    rdtsc()

    print("CPU ops done")
```

---

### Tips & Best Practices

1. **Pakai `var` untuk semua variabel** — tidak ada global var di luar main
2. **Nama fungsi lowercase** — `func hitung_luas(r):` bukan `func HitungLuas`
3. **Hindari nama variabel yang sama dengan keyword** — jangan pakai `size`, `empty`, `distance`, `add` sebagai nama var
4. **Gunakan `util::` untuk operasi yang perlu langsung ke CPU** — cocok untuk inner loop komputasi berat
5. **Fungsi pendek lebih baik** — pecah logika kompleks ke beberapa fungsi kecil
6. **Test dulu dengan `quartz check`** sebelum compile
7. **Gunakan `quartz emit-asm`** untuk lihat apa yang dihasilkan compiler

---

## 8. Contoh Program Lengkap

### Kalkulator sederhana
```quartz
func kali(a, b):
    return a * b
end!

func bagi(a, b):
    if b == 0:
        return 0
    end!
    return a / b
end!

func pangkat(base, exp):
    return pow(base, exp)
end!

int main():
    print("=== Kalkulator Quartz ===")

    var a = 12
    var b = 4

    var tambah = a + b
    var kurang = a - b
    var kali_res = kali(a, b)
    var bagi_res = bagi(a, b)
    var kuadrat = pangkat(a, 2)

    print(tambah)      // 16
    print(kurang)      // 8
    print(kali_res)    // 48
    print(bagi_res)    // 3
    print(kuadrat)     // 144

    var akar = sqrt(144)
    print(akar)        // 12
```

### Statistik sederhana
```quartz
func rata_rata(a, b, c):
    return (a + b + c) / 3
end!

func nilai_max(a, b, c):
    var m = max(a, b)
    return max(m, c)
end!

func nilai_min(a, b, c):
    var m = min(a, b)
    return min(m, c)
end!

int main():
    var n1 = 70
    var n2 = 85
    var n3 = 90

    var avg = rata_rata(n1, n2, n3)
    var mx  = nilai_max(n1, n2, n3)
    var mn  = nilai_min(n1, n2, n3)

    print(avg)   // 81
    print(mx)    // 90
    print(mn)    // 70
```

---

## 9. Arsitektur Compiler

```
Source (.qua)
    │
    ▼
┌─────────────┐
│   LEXER     │  Tokenize source → token stream
│  (C++)      │  ~0.001ms
└─────────────┘
    │
    ▼
┌─────────────┐
│   PARSER    │  Token stream → AST
│  (C++)      │  ~0.001ms
└─────────────┘
    │
    ▼
┌─────────────┐
│  CODEGEN    │  AST → NASM/GAS assembly
│  (C++)      │  Arch: x86_64 / ARM64
└─────────────┘
    │
    ▼
┌─────────────┐
│  ASSEMBLER  │  .asm/.s → .o object file
│ NASM / GAS  │  (nasm atau aarch64-linux-gnu-as)
└─────────────┘
    │
    ▼
┌─────────────┐
│   LINKER    │  .o → ELF binary
│    (ld)     │
└─────────────┘
    │
    ▼
Native Binary
```

### Komponen Internal

| Komponen | File | Fungsi |
|---|---|---|
| **Lexer** | `src/lexer.cpp` | Tokenize source, kenali 500+ keywords |
| **Parser** | `src/parser.cpp` | Build AST dari token stream |
| **CodeGen** | `src/codegen.cpp` | Generate ARM64/x86_64 assembly |
| **Compiler** | `src/compiler.cpp` | Pipeline orchestrator |
| **LogicProcessor** | `src/logic_processor.cpp` | SIMD math, ML ops |
| **HardwareDirector** | `src/logic_processor.cpp` | Low-level HW access via ASM |
| **MemoryManager** | `src/memory_manager.cpp` | Zero-leak memory tracking |
| **Runtime** | `src/runtime.cpp` | QuartzValue types |

---

## License

Quartz Programming Language v1.0.0
Built with C++ (90%) + Assembly (10%)
Target: Linux ARM64 (v8a/v9a) + x86_64
