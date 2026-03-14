#define DUCKDB_EXTENSION_MAIN

#include "pdfmd_extension.hpp"
#include "mupdf_wrapper.h"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/common/vector_operations/vector_operations.hpp"
#include "duckdb/main/connection.hpp"
#include "duckdb/catalog/catalog.hpp"
#include "duckdb/parser/parsed_data/create_scalar_function_info.hpp"
#include "duckdb/parser/parsed_data/create_table_function_info.hpp"

#include <cstdlib>
#include <string>

// pdf_to_markdown.cpp
namespace duckdb {
std::string ConvertPdfToMarkdown(const char *path);
}

namespace duckdb {

// ============================================
// スカラー関数の実装
// ============================================

// pdf_text(path) -> VARCHAR
static void PdfTextFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &path_vec = args.data[0];
	UnaryExecutor::Execute<string_t, string_t>(path_vec, result, args.size(), [&](string_t path) {
		int error = 0;
		char *text = mupdf_extract_text(path.GetString().c_str(), &error);
		if (error || !text) {
			throw IOException("pdf_text: %s", mupdf_last_error());
		}
		string result_str(text);
		free(text);
		return StringVector::AddString(result, result_str);
	});
}

// pdf_to_markdown(path) -> VARCHAR
static void PdfToMarkdownFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &path_vec = args.data[0];
	UnaryExecutor::Execute<string_t, string_t>(path_vec, result, args.size(), [&](string_t path) {
		string md = ConvertPdfToMarkdown(path.GetString().c_str());
		return StringVector::AddString(result, md);
	});
}

// pdf_page_count(path) -> INTEGER
static void PdfPageCountFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &path_vec = args.data[0];
	UnaryExecutor::Execute<string_t, int32_t>(path_vec, result, args.size(), [&](string_t path) {
		int error = 0;
		int count = mupdf_page_count(path.GetString().c_str(), &error);
		if (error) {
			throw IOException("pdf_page_count: %s", mupdf_last_error());
		}
		return count;
	});
}

// pdf_metadata(path) -> VARCHAR (JSON)
static void PdfMetadataFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &path_vec = args.data[0];
	UnaryExecutor::Execute<string_t, string_t>(path_vec, result, args.size(), [&](string_t path) {
		int error = 0;
		char *json = mupdf_metadata_json(path.GetString().c_str(), &error);
		if (error || !json) {
			throw IOException("pdf_metadata: %s", mupdf_last_error());
		}
		string result_str(json);
		free(json);
		return StringVector::AddString(result, result_str);
	});
}

// pdf_page_text(path, page_num) -> VARCHAR
static void PdfPageTextFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &path_vec = args.data[0];
	auto &page_vec = args.data[1];
	BinaryExecutor::Execute<string_t, int32_t, string_t>(path_vec, page_vec, result, args.size(),
	                                                      [&](string_t path, int32_t page_num) {
		int error = 0;
		char *text = mupdf_page_text(path.GetString().c_str(), page_num, &error);
		if (error || !text) {
			throw IOException("pdf_page_text: %s", mupdf_last_error());
		}
		string result_str(text);
		free(text);
		return StringVector::AddString(result, result_str);
	});
}

// ============================================
// テーブル関数: pdf_pages(path)
// ============================================

struct PdfPagesBindData : public TableFunctionData {
	string file_path;
	int total_pages;
};

struct PdfPagesState : public GlobalTableFunctionState {
	int current_page;
	PdfPagesState() : current_page(1) {}
};

static unique_ptr<FunctionData> PdfPagesBind(ClientContext &context, TableFunctionBindInput &input,
                                              vector<LogicalType> &return_types, vector<string> &names) {
	auto result = make_uniq<PdfPagesBindData>();
	result->file_path = input.inputs[0].GetValue<string>();

	int error = 0;
	result->total_pages = mupdf_page_count(result->file_path.c_str(), &error);
	if (error) {
		throw IOException("pdf_pages: %s", mupdf_last_error());
	}

	names.push_back("page_num");
	return_types.push_back(LogicalType::INTEGER);

	names.push_back("text");
	return_types.push_back(LogicalType::VARCHAR);

	names.push_back("width");
	return_types.push_back(LogicalType::FLOAT);

	names.push_back("height");
	return_types.push_back(LogicalType::FLOAT);

	return std::move(result);
}

static unique_ptr<GlobalTableFunctionState> PdfPagesInit(ClientContext &context, TableFunctionInitInput &input) {
	return make_uniq<PdfPagesState>();
}

static void PdfPagesScan(ClientContext &context, TableFunctionInput &data, DataChunk &output) {
	auto &bind_data = data.bind_data->Cast<PdfPagesBindData>();
	auto &state = data.global_state->Cast<PdfPagesState>();

	idx_t count = 0;
	while (state.current_page <= bind_data.total_pages && count < STANDARD_VECTOR_SIZE) {
		int page = state.current_page;
		int error = 0;

		// テキスト取得
		char *text = mupdf_page_text(bind_data.file_path.c_str(), page, &error);
		string text_str = (error || !text) ? "" : string(text);
		if (text) free(text);

		// ページサイズ取得
		float w = 0, h = 0;
		mupdf_page_dimensions(bind_data.file_path.c_str(), page, &w, &h, &error);

		output.SetValue(0, count, Value::INTEGER(page));
		output.SetValue(1, count, Value(text_str));
		output.SetValue(2, count, Value::FLOAT(w));
		output.SetValue(3, count, Value::FLOAT(h));

		count++;
		state.current_page++;
	}
	output.SetCardinality(count);
}

// ============================================
// テーブル関数: read_pdf(path)
// ============================================

struct ReadPdfBindData : public TableFunctionData {
	string file_path;
	int total_pages;
};

struct ReadPdfState : public GlobalTableFunctionState {
	int current_page;
	ReadPdfState() : current_page(1) {}
};

static unique_ptr<FunctionData> ReadPdfBind(ClientContext &context, TableFunctionBindInput &input,
                                             vector<LogicalType> &return_types, vector<string> &names) {
	auto result = make_uniq<ReadPdfBindData>();
	result->file_path = input.inputs[0].GetValue<string>();

	int error = 0;
	result->total_pages = mupdf_page_count(result->file_path.c_str(), &error);
	if (error) {
		throw IOException("read_pdf: %s", mupdf_last_error());
	}

	names.push_back("page_num");
	return_types.push_back(LogicalType::INTEGER);

	names.push_back("text");
	return_types.push_back(LogicalType::VARCHAR);

	return std::move(result);
}

static unique_ptr<GlobalTableFunctionState> ReadPdfInit(ClientContext &context, TableFunctionInitInput &input) {
	return make_uniq<ReadPdfState>();
}

static void ReadPdfScan(ClientContext &context, TableFunctionInput &data, DataChunk &output) {
	auto &bind_data = data.bind_data->Cast<ReadPdfBindData>();
	auto &state = data.global_state->Cast<ReadPdfState>();

	idx_t count = 0;
	while (state.current_page <= bind_data.total_pages && count < STANDARD_VECTOR_SIZE) {
		int page = state.current_page;
		int error = 0;

		char *text = mupdf_page_text(bind_data.file_path.c_str(), page, &error);
		string text_str = (error || !text) ? "" : string(text);
		if (text) free(text);

		output.SetValue(0, count, Value::INTEGER(page));
		output.SetValue(1, count, Value(text_str));

		count++;
		state.current_page++;
	}
	output.SetCardinality(count);
}

// ============================================
// 拡張のロード (モダンAPI)
// ============================================

void PdfmdExtension::Load(ExtensionLoader &loader) {
	// スカラー関数
	loader.RegisterFunction(ScalarFunction("pdf_text", {LogicalType::VARCHAR}, LogicalType::VARCHAR, PdfTextFunction));

	loader.RegisterFunction(
	    ScalarFunction("pdf_to_markdown", {LogicalType::VARCHAR}, LogicalType::VARCHAR, PdfToMarkdownFunction));

	loader.RegisterFunction(
	    ScalarFunction("pdf_page_count", {LogicalType::VARCHAR}, LogicalType::INTEGER, PdfPageCountFunction));

	loader.RegisterFunction(
	    ScalarFunction("pdf_metadata", {LogicalType::VARCHAR}, LogicalType::VARCHAR, PdfMetadataFunction));

	loader.RegisterFunction(ScalarFunction("pdf_page_text", {LogicalType::VARCHAR, LogicalType::INTEGER},
	                                       LogicalType::VARCHAR, PdfPageTextFunction));

	// テーブル関数: pdf_pages
	TableFunction pdf_pages_func("pdf_pages", {LogicalType::VARCHAR}, PdfPagesScan, PdfPagesBind, PdfPagesInit);
	loader.RegisterFunction(pdf_pages_func);

	// テーブル関数: read_pdf
	TableFunction read_pdf_func("read_pdf", {LogicalType::VARCHAR}, ReadPdfScan, ReadPdfBind, ReadPdfInit);
	loader.RegisterFunction(read_pdf_func);
}

std::string PdfmdExtension::Name() {
	return "pdfmd";
}

std::string PdfmdExtension::Version() const {
#ifdef EXT_VERSION_PDFMD
	return EXT_VERSION_PDFMD;
#else
	return "";
#endif
}

} // namespace duckdb

extern "C" {

DUCKDB_EXTENSION_API void pdfmd_duckdb_cpp_init(duckdb::ExtensionLoader &loader) {
	duckdb::PdfmdExtension ext;
	ext.Load(loader);
}

DUCKDB_EXTENSION_API void pdfmd_init(duckdb::DatabaseInstance &db) {
	duckdb::Connection con(db);
	con.BeginTransaction();

	auto &catalog = duckdb::Catalog::GetSystemCatalog(*con.context);

	// スカラー関数
	duckdb::CreateScalarFunctionInfo pdf_text_func(duckdb::ScalarFunction(
	    "pdf_text", {duckdb::LogicalType::VARCHAR}, duckdb::LogicalType::VARCHAR, duckdb::PdfTextFunction));
	catalog.CreateFunction(*con.context, pdf_text_func);

	duckdb::CreateScalarFunctionInfo pdf_to_markdown_func(duckdb::ScalarFunction(
	    "pdf_to_markdown", {duckdb::LogicalType::VARCHAR}, duckdb::LogicalType::VARCHAR, duckdb::PdfToMarkdownFunction));
	catalog.CreateFunction(*con.context, pdf_to_markdown_func);

	duckdb::CreateScalarFunctionInfo pdf_page_count_func(duckdb::ScalarFunction(
	    "pdf_page_count", {duckdb::LogicalType::VARCHAR}, duckdb::LogicalType::INTEGER, duckdb::PdfPageCountFunction));
	catalog.CreateFunction(*con.context, pdf_page_count_func);

	duckdb::CreateScalarFunctionInfo pdf_metadata_func(duckdb::ScalarFunction(
	    "pdf_metadata", {duckdb::LogicalType::VARCHAR}, duckdb::LogicalType::VARCHAR, duckdb::PdfMetadataFunction));
	catalog.CreateFunction(*con.context, pdf_metadata_func);

	duckdb::CreateScalarFunctionInfo pdf_page_text_func(duckdb::ScalarFunction(
	    "pdf_page_text", {duckdb::LogicalType::VARCHAR, duckdb::LogicalType::INTEGER}, duckdb::LogicalType::VARCHAR,
	    duckdb::PdfPageTextFunction));
	catalog.CreateFunction(*con.context, pdf_page_text_func);

	// テーブル関数
	duckdb::TableFunction pdf_pages_func("pdf_pages", {duckdb::LogicalType::VARCHAR}, duckdb::PdfPagesScan,
	                                     duckdb::PdfPagesBind, duckdb::PdfPagesInit);
	duckdb::CreateTableFunctionInfo pdf_pages_info(pdf_pages_func);
	catalog.CreateTableFunction(*con.context, pdf_pages_info);

	duckdb::TableFunction read_pdf_func("read_pdf", {duckdb::LogicalType::VARCHAR}, duckdb::ReadPdfScan,
	                                    duckdb::ReadPdfBind, duckdb::ReadPdfInit);
	duckdb::CreateTableFunctionInfo read_pdf_info(read_pdf_func);
	catalog.CreateTableFunction(*con.context, read_pdf_info);

	con.Commit();
}

DUCKDB_EXTENSION_API const char *pdfmd_version() {
	return duckdb::DuckDB::LibraryVersion();
}

}
