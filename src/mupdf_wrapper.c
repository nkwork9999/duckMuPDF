#include "include/mupdf_wrapper.h"
#include <mupdf/fitz.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static __thread char last_error_buf[512] = {0};

const char *mupdf_last_error(void) {
	return last_error_buf;
}

static void set_error(const char *msg) {
	snprintf(last_error_buf, sizeof(last_error_buf), "%s", msg);
}

char *mupdf_extract_text(const char *path, int *out_error) {
	fz_context *ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT);
	if (!ctx) {
		set_error("Failed to create MuPDF context");
		*out_error = 1;
		return NULL;
	}

	fz_document *doc = NULL;
	fz_buffer *buf = fz_new_buffer(ctx, 1024);
	fz_output *out = fz_new_output_with_buffer(ctx, buf);
	*out_error = 0;

	fz_try(ctx) {
		fz_register_document_handlers(ctx);
		doc = fz_open_document(ctx, path);
		int pages = fz_count_pages(ctx, doc);

		for (int i = 0; i < pages; i++) {
			fz_page *page = fz_load_page(ctx, doc, i);
			fz_stext_page *stext = fz_new_stext_page_from_page(ctx, page, NULL);
			fz_print_stext_page_as_text(ctx, out, stext);
			fz_write_string(ctx, out, "\n");
			fz_drop_stext_page(ctx, stext);
			fz_drop_page(ctx, page);
		}
	}
	fz_always(ctx) {
		if (doc) fz_drop_document(ctx, doc);
	}
	fz_catch(ctx) {
		set_error(fz_caught_message(ctx));
		*out_error = 1;
		fz_drop_output(ctx, out);
		fz_drop_buffer(ctx, buf);
		fz_drop_context(ctx);
		return NULL;
	}

	fz_close_output(ctx, out);
	fz_drop_output(ctx, out);

	/* buffer -> malloc'd string */
	unsigned char *data = NULL;
	size_t len = fz_buffer_extract(ctx, buf, &data);
	char *result = (char *)malloc(len + 1);
	memcpy(result, data, len);
	result[len] = '\0';
	free(data);

	fz_drop_buffer(ctx, buf);
	fz_drop_context(ctx);
	return result;
}

int mupdf_page_count(const char *path, int *out_error) {
	fz_context *ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT);
	if (!ctx) {
		set_error("Failed to create MuPDF context");
		*out_error = 1;
		return 0;
	}

	int pages = 0;
	fz_document *doc = NULL;
	*out_error = 0;

	fz_try(ctx) {
		fz_register_document_handlers(ctx);
		doc = fz_open_document(ctx, path);
		pages = fz_count_pages(ctx, doc);
	}
	fz_always(ctx) {
		if (doc) fz_drop_document(ctx, doc);
	}
	fz_catch(ctx) {
		set_error(fz_caught_message(ctx));
		*out_error = 1;
	}

	fz_drop_context(ctx);
	return pages;
}

char *mupdf_page_text(const char *path, int page_num, int *out_error) {
	fz_context *ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT);
	if (!ctx) {
		set_error("Failed to create MuPDF context");
		*out_error = 1;
		return NULL;
	}

	fz_document *doc = NULL;
	fz_buffer *buf = fz_new_buffer(ctx, 1024);
	fz_output *out = fz_new_output_with_buffer(ctx, buf);
	*out_error = 0;

	fz_try(ctx) {
		fz_register_document_handlers(ctx);
		doc = fz_open_document(ctx, path);
		int pages = fz_count_pages(ctx, doc);

		if (page_num < 1 || page_num > pages) {
			fz_throw(ctx, FZ_ERROR_ARGUMENT, "Page number %d out of range (1-%d)", page_num, pages);
		}

		fz_page *page = fz_load_page(ctx, doc, page_num - 1);
		fz_stext_page *stext = fz_new_stext_page_from_page(ctx, page, NULL);
		fz_print_stext_page_as_text(ctx, out, stext);
		fz_drop_stext_page(ctx, stext);
		fz_drop_page(ctx, page);
	}
	fz_always(ctx) {
		if (doc) fz_drop_document(ctx, doc);
	}
	fz_catch(ctx) {
		set_error(fz_caught_message(ctx));
		*out_error = 1;
		fz_drop_output(ctx, out);
		fz_drop_buffer(ctx, buf);
		fz_drop_context(ctx);
		return NULL;
	}

	fz_close_output(ctx, out);
	fz_drop_output(ctx, out);

	unsigned char *data2 = NULL;
	size_t len = fz_buffer_extract(ctx, buf, &data2);
	char *result = (char *)malloc(len + 1);
	memcpy(result, data2, len);
	result[len] = '\0';
	free(data2);

	fz_drop_buffer(ctx, buf);
	fz_drop_context(ctx);
	return result;
}

char *mupdf_metadata_json(const char *path, int *out_error) {
	fz_context *ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT);
	if (!ctx) {
		set_error("Failed to create MuPDF context");
		*out_error = 1;
		return NULL;
	}

	fz_document *doc = NULL;
	*out_error = 0;

	char title[256] = {0};
	char author[256] = {0};
	char subject[256] = {0};
	char creator[256] = {0};
	char producer[256] = {0};
	char creation_date[256] = {0};
	char mod_date[256] = {0};

	fz_try(ctx) {
		fz_register_document_handlers(ctx);
		doc = fz_open_document(ctx, path);

		fz_lookup_metadata(ctx, doc, FZ_META_INFO_TITLE, title, sizeof(title));
		fz_lookup_metadata(ctx, doc, FZ_META_INFO_AUTHOR, author, sizeof(author));
		fz_lookup_metadata(ctx, doc, "info:Subject", subject, sizeof(subject));
		fz_lookup_metadata(ctx, doc, "info:Creator", creator, sizeof(creator));
		fz_lookup_metadata(ctx, doc, "info:Producer", producer, sizeof(producer));
		fz_lookup_metadata(ctx, doc, "info:CreationDate", creation_date, sizeof(creation_date));
		fz_lookup_metadata(ctx, doc, "info:ModDate", mod_date, sizeof(mod_date));
	}
	fz_always(ctx) {
		if (doc) fz_drop_document(ctx, doc);
	}
	fz_catch(ctx) {
		set_error(fz_caught_message(ctx));
		*out_error = 1;
		fz_drop_context(ctx);
		return NULL;
	}

	/* JSON 文字列を組み立て */
	size_t json_size = 2048;
	char *json = (char *)malloc(json_size);
	snprintf(json, json_size,
	         "{\"title\":\"%s\",\"author\":\"%s\",\"subject\":\"%s\","
	         "\"creator\":\"%s\",\"producer\":\"%s\","
	         "\"creation_date\":\"%s\",\"mod_date\":\"%s\"}",
	         title, author, subject, creator, producer, creation_date, mod_date);

	fz_drop_context(ctx);
	return json;
}

void mupdf_page_dimensions(const char *path, int page_num, float *width, float *height, int *out_error) {
	fz_context *ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT);
	if (!ctx) {
		set_error("Failed to create MuPDF context");
		*out_error = 1;
		return;
	}

	fz_document *doc = NULL;
	*out_error = 0;

	fz_try(ctx) {
		fz_register_document_handlers(ctx);
		doc = fz_open_document(ctx, path);
		int pages = fz_count_pages(ctx, doc);

		if (page_num < 1 || page_num > pages) {
			fz_throw(ctx, FZ_ERROR_ARGUMENT, "Page number %d out of range (1-%d)", page_num, pages);
		}

		fz_page *page = fz_load_page(ctx, doc, page_num - 1);
		fz_rect bounds = fz_bound_page(ctx, page);
		*width = bounds.x1 - bounds.x0;
		*height = bounds.y1 - bounds.y0;
		fz_drop_page(ctx, page);
	}
	fz_always(ctx) {
		if (doc) fz_drop_document(ctx, doc);
	}
	fz_catch(ctx) {
		set_error(fz_caught_message(ctx));
		*out_error = 1;
	}

	fz_drop_context(ctx);
}
