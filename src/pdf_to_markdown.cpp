#include "include/mupdf_wrapper.h"
#include <string>
#include <sstream>
#include <cstdlib>

namespace duckdb {

// 簡易的な PDF→Markdown 変換
// MuPDF のテキスト抽出結果をMarkdownとして整形する
std::string ConvertPdfToMarkdown(const char *path) {
	int error = 0;
	int pages = mupdf_page_count(path, &error);
	if (error) {
		return std::string("Error: ") + mupdf_last_error();
	}

	std::ostringstream md;

	for (int i = 1; i <= pages; i++) {
		char *text = mupdf_page_text(path, i, &error);
		if (error || !text) {
			md << "\n\n---\n\n> Error reading page " << i << "\n";
			continue;
		}

		if (i > 1) {
			md << "\n\n---\n\n";
		}

		// ページテキストをそのまま出力（基本変換）
		// 行頭の空白が多い行はインデントブロックとして扱う
		std::string page_text(text);
		free(text);

		std::istringstream stream(page_text);
		std::string line;
		bool prev_empty = false;

		while (std::getline(stream, line)) {
			// 空行の重複を抑制
			if (line.empty() || line.find_first_not_of(" \t\r\n") == std::string::npos) {
				if (!prev_empty) {
					md << "\n";
					prev_empty = true;
				}
				continue;
			}
			prev_empty = false;
			md << line << "\n";
		}
	}

	return md.str();
}

} // namespace duckdb
