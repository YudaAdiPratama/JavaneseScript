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

## Menjalankan

Butuh Node.js 18 ke atas, tanpa dependensi lain.

```sh
node bin/jws.js examples/halo.jws   # jalankan berkas .jws
node bin/jws.js                     # REPL interaktif
npm test                            # jalankan tes
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

JavaneseScript punya tata bahasa dan implementasi sendiri, bukan JavaScript yang kata kuncinya diganti:

1. `src/lexer.js` memecah kode menjadi token.
2. `src/parser.js` menyusun token jadi pohon sintaks (AST) sesuai tata bahasa JavaneseScript.
3. `src/interpreter.js` menjalankan AST langsung, lengkap dengan lingkup variabel, closure, dan penanganan kesalahan.
