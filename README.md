# duckMuPDF

DuckDB extension that binds MuPDF C API for reading PDF files directly from SQL.

## Functions

### Scalar Functions

| Function | Description | Returns |
|----------|-------------|---------|
| `pdf_text(path)` | Extract plain text from PDF | VARCHAR |
| `pdf_to_markdown(path)` | Convert PDF to Markdown | VARCHAR |
| `pdf_page_count(path)` | Get number of pages | INTEGER |
| `pdf_metadata(path)` | Get metadata as JSON | VARCHAR |
| `pdf_page_text(path, page_num)` | Extract text from specific page | VARCHAR |

### Table Functions

| Function | Description | Columns |
|----------|-------------|---------|
| `pdf_pages(path)` | Each page as a row | page_num, text, width, height |
| `read_pdf(path)` | Structured page data | page_num, text |

## Usage

```sql
LOAD 'pdfmd';

-- Extract text
SELECT pdf_text('/path/to/document.pdf');

-- Page count
SELECT pdf_page_count('/path/to/document.pdf');

-- Metadata (JSON)
SELECT pdf_metadata('/path/to/document.pdf');

-- Convert to Markdown
SELECT pdf_to_markdown('/path/to/document.pdf');

-- Specific page
SELECT pdf_page_text('/path/to/document.pdf', 1);

-- All pages as table
SELECT * FROM pdf_pages('/path/to/document.pdf');

-- Batch processing with glob
SELECT filename, pdf_page_count(filename)
FROM glob('/path/to/docs/*.pdf') t(filename);
```

## Build

### Prerequisites

- C/C++ compiler (gcc/clang)
- CMake >= 3.5
- MuPDF library
- Git

### Install MuPDF

```bash
# macOS
brew install mupdf

# Ubuntu/Debian
sudo apt-get install libmupdf-dev
```

### Build Extension

```bash
git clone --recurse-submodules https://github.com/nkwork9999/duckMuPDF.git
cd duckMuPDF
make release
```

### Load in DuckDB

```bash
./build/release/duckdb -unsigned
```

```sql
LOAD 'pdfmd';
SELECT pdf_text('/path/to/document.pdf');
```

## Architecture

```
src/
├── pdfmd_extension.cpp       # Extension entry point & function registration
├── include/
│   ├── pdfmd_extension.hpp   # Extension class header
│   └── mupdf_wrapper.h      # MuPDF C API wrapper header
├── mupdf_wrapper.c           # MuPDF C API implementation
└── pdf_to_markdown.cpp       # Markdown conversion logic
```

## License

MIT (MuPDF is AGPL / commercial license)
