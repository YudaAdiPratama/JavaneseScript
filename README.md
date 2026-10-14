# JavaneseScript

Bahasa pemrograman dengan tembung kunci **bahasa Jawa** (huruf Latin biasa, jadi gampang diketik).

```
gawe faktorial(n) {
    yen n <= 1 { balekna 1 }
    balekna n * faktorial(n - 1)
}

saben k ing urutan(1, 7) {
    tulis k, "! =", faktorial(k)
}
```

Output:

```
1 ! = 1
2 ! = 2
...
6 ! = 720
```

## Memasang

JavaneseScript ditulis dalam C dan tidak butuh apa-apa selain compiler C (gcc atau clang).

```sh
make            # menghasilkan program ./jws
make test       # menjalankan semua tes
```

Program `jws` yang dihasilkan berdiri sendiri, jadi bisa disalin ke mana saja (misalnya `/usr/local/bin`).

## Menjalankan

```sh
./jws examples/halo.jws   # jalankan berkas .jws
./jws                     # REPL interaktif
```

## Tembung kunci

| Tembung | Arti |
|---|---|
| `simpen` | buat variabel baru |
| `tulis` | cetak ke layar |
| `yen` | jika |
| `liyane` | selain itu (else) |
| `salawase` | selama (while) |
| `saben … ing` | untuk setiap … di (for each) |
| `gawe` | buat fungsi |
| `balekna` | kembalikan nilai (return) |
| `mandheg` | hentikan perulangan (break) |
| `terus` | lanjut ke putaran berikutnya (continue) |
| `bener` / `luput` | benar / salah |
| `kosong` | tidak ada nilai (null) |
| `lan` / `utawa` / `ora` | dan / atau / tidak |

## Fungsi bawaan

| Fungsi | Arti |
|---|---|
| `dawa(x)` | panjang daftar atau teks |
| `lebokna(daftar, x)` | tambahkan `x` ke akhir daftar |
| `urutan(a, b)` | daftar angka dari `a` sampai `b - 1` |
| `angka(x)` | ubah teks jadi angka |
| `tembung(x)` | ubah nilai jadi teks |
| `waca(pesan)` | baca satu baris masukan dari pengguna |
| `jinis(x)` | jenis nilai: `angka`, `tembung`, `logika`, `daftar`, `gawe`, `kosong` |

## Tata bahasa singkat

- **Teks**: diapit `"…"`, mendukung `\n`, `\t`, `\"`, `\\`.
- **Daftar**: `[1, 2, 3]`, akses dengan `d[0]`, ubah dengan `d[0] = 9`.
- **Operator**: `+ - * / %`, `== != < <= > >=`. `+` juga menyambung teks dan daftar.
- **Komentar**: `// sampai akhir baris`.
- **Blok**: `{ … }`. `;` boleh dipakai untuk memisah pernyataan dalam satu baris.
- **Fungsi** adalah nilai dan mendukung closure: `simpen dobel = gawe(x) { balekna x * 2 }`.
- Pesan kesalahan ditulis dalam bahasa Jawa dan menyertakan nomor baris.

## Cara kerja

Kode sumbernya ada di `src/`:

1. `lexer.c` memecah kode menjadi token.
2. `parser.c` menyusun token jadi pohon sintaks (AST) sesuai tata bahasa JavaneseScript.
3. `interp.c` menjalankan AST: lingkup variabel, closure, fungsi bawaan, dan pesan kesalahan.
4. `value.c` mengurus nilai (angka, teks, daftar, fungsi), format output, dan *garbage collector* yang membersihkan memori yang sudah tidak dipakai.
5. `main.c` adalah program `jws` itu sendiri: menjalankan berkas atau membuka REPL.

Rekursi bisa sangat dalam (lebih dari 100.000 tingkat). Kalau terlalu dalam, program berhenti dengan pesan `rekursi kakehan`, tidak crash.

## Tes

`tests/run.sh` menjalankan setiap berkas `.jws` di `examples/` dan `tests/`, lalu membandingkan output, pesan kesalahan, dan kode keluarnya dengan berkas `.out`.
