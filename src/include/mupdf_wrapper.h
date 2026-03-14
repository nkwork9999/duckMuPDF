#ifndef MUPDF_WRAPPER_H
#define MUPDF_WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

/* PDF からプレーンテキストを抽出 (呼び出し側が free() すること) */
char *mupdf_extract_text(const char *path, int *out_error);

/* PDF のページ数を取得 */
int mupdf_page_count(const char *path, int *out_error);

/* 指定ページのテキストを抽出 (呼び出し側が free() すること) */
char *mupdf_page_text(const char *path, int page_num, int *out_error);

/* PDF メタデータを JSON 文字列で取得 (呼び出し側が free() すること) */
char *mupdf_metadata_json(const char *path, int *out_error);

/* 指定ページの幅と高さを取得 */
void mupdf_page_dimensions(const char *path, int page_num, float *width, float *height, int *out_error);

/* 最後のエラーメッセージを取得 */
const char *mupdf_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* MUPDF_WRAPPER_H */
