/*
 * Xhtml.cpp
 * Copyright (C) 2022 pulkomandy <pulkomandy@pulkomandy.tk>
 *
 * Distributed under terms of the MIT license.
 */

#include "Xhtml.h"

#include <sstream>

#include <libwapcaplet/libwapcaplet.h>


/* This macro is used to silence compiler warnings about unused function
 * arguments. */
#define UNUSED(x) ((x) = (x))


/* Function declarations. */
static css_error resolve_url(void *pw,
		const char *base, lwc_string *rel, lwc_string **abs);

static css_error node_name(void *pw, void *node,
		css_qname *qname);
static css_error node_classes(void *pw, void *node,
		lwc_string ***classes, uint32_t *n_classes);
static css_error node_id(void *pw, void *node,
		lwc_string **id);
static css_error named_ancestor_node(void *pw, void *node,
		const css_qname *qname,
		void **ancestor);
static css_error named_parent_node(void *pw, void *node,
		const css_qname *qname,
		void **parent);
static css_error named_sibling_node(void *pw, void *node,
		const css_qname *qname,
		void **sibling);
static css_error named_generic_sibling_node(void *pw, void *node,
		const css_qname *qname,
		void **sibling);
static css_error parent_node(void *pw, void *node, void **parent);
static css_error sibling_node(void *pw, void *node, void **sibling);
static css_error node_has_name(void *pw, void *node,
		const css_qname *qname,
		bool *match);
static css_error node_has_class(void *pw, void *node,
		lwc_string *name,
		bool *match);
static css_error node_has_id(void *pw, void *node,
		lwc_string *name,
		bool *match);
static css_error node_has_attribute(void *pw, void *node,
		const css_qname *qname,
		bool *match);
static css_error node_has_attribute_equal(void *pw, void *node,
		const css_qname *qname,
		lwc_string *value,
		bool *match);
static css_error node_has_attribute_dashmatch(void *pw, void *node,
		const css_qname *qname,
		lwc_string *value,
		bool *match);
static css_error node_has_attribute_includes(void *pw, void *node,
		const css_qname *qname,
		lwc_string *value,
		bool *match);
static css_error node_has_attribute_prefix(void *pw, void *node,
		const css_qname *qname,
		lwc_string *value,
		bool *match);
static css_error node_has_attribute_suffix(void *pw, void *node,
		const css_qname *qname,
		lwc_string *value,
		bool *match);
static css_error node_has_attribute_substring(void *pw, void *node,
		const css_qname *qname,
		lwc_string *value,
		bool *match);
static css_error node_is_root(void *pw, void *node, bool *match);
static css_error node_count_siblings(void *pw, void *node,
		bool same_name, bool after, int32_t *count);
static css_error node_is_empty(void *pw, void *node, bool *match);
static css_error node_is_link(void *pw, void *node, bool *match);
static css_error node_is_visited(void *pw, void *node, bool *match);
static css_error node_is_hover(void *pw, void *node, bool *match);
static css_error node_is_active(void *pw, void *node, bool *match);
static css_error node_is_focus(void *pw, void *node, bool *match);
static css_error node_is_enabled(void *pw, void *node, bool *match);
static css_error node_is_disabled(void *pw, void *node, bool *match);
static css_error node_is_checked(void *pw, void *node, bool *match);
static css_error node_is_target(void *pw, void *node, bool *match);
static css_error node_is_lang(void *pw, void *node,
		lwc_string *lang, bool *match);
static css_error node_presentational_hint(void *pw, void *node,
		uint32_t *nhints, css_hint **hints);
static css_error ua_default_for_property(void *pw, uint32_t property,
		css_hint *hint);
static css_error set_libcss_node_data(void *pw, void *n,
		void *libcss_node_data);
static css_error get_libcss_node_data(void *pw, void *n,
		void **libcss_node_data);

/* Table of function pointers for the LibCSS Select API. */
static css_select_handler select_handler = {
	CSS_SELECT_HANDLER_VERSION_1,

	node_name,
	node_classes,
	node_id,
	named_ancestor_node,
	named_parent_node,
	named_sibling_node,
	named_generic_sibling_node,
	parent_node,
	sibling_node,
	node_has_name,
	node_has_class,
	node_has_id,
	node_has_attribute,
	node_has_attribute_equal,
	node_has_attribute_dashmatch,
	node_has_attribute_includes,
	node_has_attribute_prefix,
	node_has_attribute_suffix,
	node_has_attribute_substring,
	node_is_root,
	node_count_siblings,
	node_is_empty,
	node_is_link,
	node_is_visited,
	node_is_hover,
	node_is_active,
	node_is_focus,
	node_is_enabled,
	node_is_disabled,
	node_is_checked,
	node_is_target,
	node_is_lang,
	node_presentational_hint,
	ua_default_for_property,
	set_libcss_node_data,
	get_libcss_node_data
};


static css_error parse_custom_css_color(void* context, lwc_string* data, uint32_t* result)
{
	CustomCssColor* colors = (CustomCssColor*) context;

	for (int i = 0; colors[i].colorString != NULL; i++) {
		bool match = false;
		lwc_string_caseless_isequal(data, colors[i].colorString, &match);
		if (match) {
			*result = colors[i].cssColor;
			return CSS_OK;
		}
	}

	printf("Unknown color %s\n", lwc_string_data(data));
	return CSS_INVALID;
}


XhtmlProcessor::XhtmlProcessor()
{
	/* prepare a selection context containing the stylesheet */
	css_error code = css_select_ctx_create(&select_ctx);
	if (code != CSS_OK)
		throw "Can't create CSS context";

	params = {};
	params.params_version = CSS_STYLESHEET_PARAMS_VERSION_1;
	params.level = CSS_LEVEL_21;
	params.charset = "UTF-8";
	params.url = "inline";
	params.title = "XMPP-IM inline style";
	params.resolve = resolve_url;

	// Biboumi incorrectly uses a non(existing color "lightmagenta". As a workaround, we handle it
	// in the custom color parser.
	lwc_intern_string("lightmagenta", strlen("lightmagenta"), &colors[0].colorString);
	colors[0].cssColor = 0xFFFF6B84; // same as "hotpink"

	// Allow use of "linktext" (defined in css-color-4 system colors). We use it in the baseStyles
	// below to have links use the correct color defined in user preferences.
	// TODO should we also have "canvastext" mapped to B_DOCUMENT_TEXT_COLOR?
	lwc_intern_string("linktext", strlen("linktext"), &colors[1].colorString);
	rgb_color systemLinkColor = ui_color(B_LINK_TEXT_COLOR);
	colors[1].cssColor = (systemLinkColor.alpha << 24) | (systemLinkColor.red << 16)
		| (systemLinkColor.green << 8) | systemLinkColor.blue;

	colors[2].colorString = NULL;

	params.color = parse_custom_css_color;
	params.color_pw = colors;

	// Based on https://git.netsurf-browser.org/netsurf.git/tree/resources/default.css
	// Removed all unneeded things (keep only recommended xhtml-im stuff)
	// FIXME Normally the style for links should apply only to a:link (a element with href
	// attribute) but it's not clear how libcss decide if an a tag is a link or not (possibly
	// it is up to us to tell it so). For now we just have all "a" elements rendered as links
	// (including anchors and other stuff). This is good enough until we find something that uses
	// a elements in XHTML-IM for something else than links.
	const char* baseStyles = R"(
		a { color: linktext; text-decoration: underline; }
		body { display: block; margin: 8px; line-height: 1.33; }
		blockquote { display: block; margin: 1.12em 40px;}
		cite { font-style: italic; }
		em { font-style: italic; }
		img { color: #888; }
		li { display: list-item; }
		ol { display: block; padding-left: 40px; margin: 1.12em 0; list-style-type: decimal; }
		p { display: block; margin: 1.12em 0; }
		strong { font-weight: bold; }
		ul { display: block; padding-left: 40px; margin: 1.12em 0; list-style-type: disc; }
	)";

	css_error error;
	error = css_stylesheet_create(&params, &base_sheet);
	if (error != CSS_OK)
		printf("Fail to create basesheet: %s\n", css_error_to_string(error));
	error = css_stylesheet_append_data(base_sheet, (const uint8_t*)baseStyles, strlen(baseStyles));
	if (error != CSS_OK && error != CSS_NEEDDATA)
		printf("Fail to parse basesheet: %s\n", css_error_to_string(error));
	error = css_stylesheet_data_done(base_sheet);
	if (error != CSS_OK)
		printf("Fail to finalize basesheet: %s\n", css_error_to_string(error));

	error = css_select_ctx_append_sheet(select_ctx, base_sheet, CSS_ORIGIN_UA, "screen");
	if (error != CSS_OK)
		printf("Fail to add basesheet to context: %s\n", css_error_to_string(error));

	// We will reuse the same parameters for other stylesheets, but they will all be inline style
	params.inline_style = true;
}


XhtmlProcessor::~XhtmlProcessor()
{
	/* free everything and shut down libcss */
	for (int i = 0; colors[i].colorString; i++) {
		lwc_string_unref(colors[i].colorString);
	}
	css_select_ctx_destroy(select_ctx);
	css_stylesheet_destroy(base_sheet);
}


void
XhtmlProcessor::ProcessXhtml(const gloox::Tag* tag, CharacterStyle characterStyle, std::list<TextSpan>& spans)
{
	css_media media = { };
	media.type = CSS_MEDIA_SCREEN;

	const gloox::Tag::NodeList& children = tag->nodes();
	std::string target;

	for (auto child: children) {
		if (child->type == gloox::Tag::TypeString) {
			TextSpan span(child->str->c_str(), characterStyle);
			spans.push_back(span);
		} else {
			//size_t startOffset;
			// Start with a default formatting
			//startOffset = output.tellp();
			characterStyle.SetFont(be_plain_font);
			characterStyle.SetForegroundColor(B_DOCUMENT_TEXT_COLOR);

			css_stylesheet* sheet = NULL;
			std::string inlineStyle = child->tag->findAttribute("style");
			if (!inlineStyle.empty()) {
				css_stylesheet_create(&params, &sheet);
				css_stylesheet_append_data(sheet, (uint8_t*)inlineStyle.c_str(), inlineStyle.length());
				css_stylesheet_data_done(sheet);
			}

			css_unit_ctx unit_ctx {800, 600, 12, 10, 96, NULL, NULL, NULL };

			css_select_results *style;
			css_error code = css_select_style(select_ctx, child->tag,
				&unit_ctx, &media, sheet,
				&select_handler, 0,
				&style);

			if (code == CSS_OK) {
				css_color color_shade;
				uint8_t color_type;
				color_type = css_computed_color(
						style->styles[CSS_PSEUDO_ELEMENT_NONE],
						&color_shade);
				if (color_type == CSS_COLOR_INHERIT)
					printf("color of element is 'inherit'\n");
				else {
					characterStyle.SetForegroundColor(color_shade >> 16, color_shade >> 8, color_shade,
						color_shade >> 24);
				}

				BFont font = characterStyle.Font();
				uint32 face = characterStyle.Font().Face();
				face &= ~(B_BOLD_FACE | B_REGULAR_FACE | B_LIGHT_FACE | B_HEAVY_FACE);

				int decoration = css_computed_text_decoration(style->styles[CSS_PSEUDO_ELEMENT_NONE]);
				if (decoration & CSS_TEXT_DECORATION_NONE)
					face &= ~(B_UNDERSCORE_FACE | B_NEGATIVE_FACE | B_OUTLINED_FACE | B_STRIKEOUT_FACE | B_CONDENSED_FACE);

				if (decoration & CSS_TEXT_DECORATION_UNDERLINE)
					face |= B_UNDERSCORE_FACE;
				else
					face &= ~B_UNDERSCORE_FACE;

				if (decoration & CSS_TEXT_DECORATION_LINE_THROUGH)
					face |= B_STRIKEOUT_FACE;
				else
					face &= ~B_STRIKEOUT_FACE;

				int weight = css_computed_font_weight(style->styles[CSS_PSEUDO_ELEMENT_NONE]);

				if (weight == CSS_FONT_WEIGHT_NORMAL)
					face |= B_REGULAR_FACE;
				else if (weight == CSS_FONT_WEIGHT_BOLD)
					face |= B_BOLD_FACE;
				else if (weight == CSS_FONT_WEIGHT_BOLDER)
					face |= B_HEAVY_FACE;
				else if (weight == CSS_FONT_WEIGHT_LIGHTER)
					face |= B_LIGHT_FACE;
				else
					face |= B_REGULAR_FACE;

				int fontStyle = css_computed_font_style(style->styles[CSS_PSEUDO_ELEMENT_NONE]);
				if (fontStyle == CSS_FONT_STYLE_ITALIC) {
					face |= B_ITALIC_FACE;
					face &= ~B_REGULAR_FACE;
				} else {
					face &= ~B_ITALIC_FACE;
				}

				font.SetFace(face);
				characterStyle.SetFont(font);

				// No support for overline, blink, and inherit
				// TODO handle other css attributes:
				// - background-color
				// - font-famiily, font-size, font-style, font-weight
				// - margin-left, margin-right, text-align
				css_select_results_destroy(style);
				if (sheet != NULL)
					css_stylesheet_destroy(sheet);
			}
			// TODO get code from fRiSS for special handling of more of the allowed tags:
			// blockquote (with cite attribute), br, cite, em, img, li, ol, p, ul
			// TODO handle link attributes: type (MIME type of link, could be used to find the
			// preferred app? but that would be useful only if we handle downloads ourselves and
			// then directly hand it over to the app that can open the file. Unless we curlfs all
			// the things?)
			// TODO handle body xmllang attribute if present (select an useful language depending
			// on user locale preference?)
			// TODO handle img attributes: alt, height, src, width (do we want to be able to show
			// images in BTextView? fRiSS has some hacky code for it that we could maybe reuse)
			ProcessXhtml(child->tag, characterStyle, spans);

			// Insert another run with default style when exiting the tag
			// FIXME do we need to stack different styles together?
			// For example how is this handled:
			// <span style="color: hotpink">Some text with <em>extra attributes</em></span>
			// Will libcss already tell us that the inner span is both bold and hotpink, or do
			// we have to keep track of that?
			// TODO when inserting a run, don't do it if the next one starts at the same offset
			// This will happen if there is something like this:
			// <span style="xxx"><span style="yyy">Some text</span></span>
			// Each span then gets a run but the "xxx" span will end up being empty.

			if (child->tag->name() == "a") {
				target = child->tag->findAttribute("href");

				if (!target.empty()) {
					spans.back().SetCursor(BCursor(B_CURSOR_ID_FOLLOW_LINK));
					BMessage* message = new BMessage('hlnk');
					message->AddString("target", target.c_str());
					spans.back().SetClickMessage(message);
				}
			}

			// Back to default when exiting a tag
			characterStyle = CharacterStyle();
			characterStyle.SetFont(be_plain_font);
			characterStyle.SetForegroundColor(B_DOCUMENT_TEXT_COLOR);
		}
	}
}

/* Select handlers. These are added to the select_ctx and called by libcss to get info about a node.
 *
 * The parameters are:
 * - pw: the global context we defined. Currently unused
 * - n: the node being analyzed. In our case this can be casted to a gloox::Tag*
 * - Anything else will be output parameters that the function is expected to fill.
 *
 * Most of them are empty because pretty much all of xhtml-im is done solely based on the element
 * name (a, p, span, ...) and inline styles. But if we find something else is needed (id, classes,
 * ...) we can add that by filling in the corresponding function. For example implementing
 * node_has_attribute would help detecting <a/> tags that do have an href and should thus be
 * considered as links (right now, even <a/> tags used as anchors will be styled as links).
 */
css_error resolve_url(void *pw,
		const char *base, lwc_string *rel, lwc_string **abs)
{
	UNUSED(pw);
	UNUSED(base);

	/* About as useless as possible */
	*abs = lwc_string_ref(rel);

	return CSS_OK;
}

css_error node_name(void *pw, void *n, css_qname *qname)
{
	gloox::Tag *node = (gloox::Tag*)n;

	UNUSED(pw);

	lwc_intern_string(node->name().c_str(), node->name().length(), &qname->name);

	return CSS_OK;
}

css_error node_classes(void *pw, void *n,
		lwc_string ***classes, uint32_t *n_classes)
{
	UNUSED(pw);
	UNUSED(n);
	*classes = NULL;
	*n_classes = 0;
	return CSS_OK;
}

css_error node_id(void *pw, void *n, lwc_string **id)
{
	UNUSED(pw);
	UNUSED(n);
	*id = NULL;
	return CSS_OK;
}

css_error named_ancestor_node(void *pw, void *n,
		const css_qname *qname,
		void **ancestor)
{
	UNUSED(pw);
	UNUSED(n);
	UNUSED(qname);
	*ancestor = NULL;
	return CSS_OK;
}

css_error named_parent_node(void *pw, void *n,
		const css_qname *qname,
		void **parent)
{
	UNUSED(pw);
	UNUSED(n);
	UNUSED(qname);
	*parent = NULL;
	return CSS_OK;
}

css_error named_generic_sibling_node(void *pw, void *n,
		const css_qname *qname,
		void **sibling)
{
	UNUSED(pw);
	UNUSED(n);
	UNUSED(qname);
	*sibling = NULL;
	return CSS_OK;
}

css_error named_sibling_node(void *pw, void *n,
		const css_qname *qname,
		void **sibling)
{
	UNUSED(pw);
	UNUSED(n);
	UNUSED(qname);
	*sibling = NULL;
	return CSS_OK;
}

css_error parent_node(void *pw, void *n, void **parent)
{
	UNUSED(pw);
	UNUSED(n);
	*parent = NULL;
	return CSS_OK;
}

css_error sibling_node(void *pw, void *n, void **sibling)
{
	UNUSED(pw);
	UNUSED(n);
	*sibling = NULL;
	return CSS_OK;
}

css_error node_has_name(void *pw, void *n,
		const css_qname *qname,
		bool *match)
{
	UNUSED(pw);
	UNUSED(n);
	UNUSED(qname);
	UNUSED(match);
	return CSS_OK;
}

css_error node_has_class(void *pw, void *n,
		lwc_string *name,
		bool *match)
{
	UNUSED(pw);
	UNUSED(n);
	UNUSED(name);
	*match = false;
	return CSS_OK;
}

css_error node_has_id(void *pw, void *n,
		lwc_string *name,
		bool *match)
{
	UNUSED(pw);
	UNUSED(n);
	UNUSED(name);
	*match = false;
	return CSS_OK;
}

css_error node_has_attribute(void *pw, void *n,
		const css_qname *qname,
		bool *match)
{
	// TODO fill in to allow detection of the "href" attribute for links
	UNUSED(pw);
	UNUSED(n);
	UNUSED(qname);
	*match = false;
	return CSS_OK;
}

css_error node_has_attribute_equal(void *pw, void *n,
		const css_qname *qname,
		lwc_string *value,
		bool *match)
{
	UNUSED(pw);
	UNUSED(n);
	UNUSED(qname);
	UNUSED(value);
	*match = false;
	return CSS_OK;
}

css_error node_has_attribute_dashmatch(void *pw, void *n,
		const css_qname *qname,
		lwc_string *value,
		bool *match)
{
	UNUSED(pw);
	UNUSED(n);
	UNUSED(qname);
	UNUSED(value);
	*match = false;
	return CSS_OK;
}

css_error node_has_attribute_includes(void *pw, void *n,
		const css_qname *qname,
		lwc_string *value,
		bool *match)
{
	UNUSED(pw);
	UNUSED(n);
	UNUSED(qname);
	UNUSED(value);
	*match = false;
	return CSS_OK;
}

css_error node_has_attribute_prefix(void *pw, void *n,
		const css_qname *qname,
		lwc_string *value,
		bool *match)
{
	UNUSED(pw);
	UNUSED(n);
	UNUSED(qname);
	UNUSED(value);
	*match = false;
	return CSS_OK;
}

css_error node_has_attribute_suffix(void *pw, void *n,
		const css_qname *qname,
		lwc_string *value,
		bool *match)
{
	UNUSED(pw);
	UNUSED(n);
	UNUSED(qname);
	UNUSED(value);
	*match = false;
	return CSS_OK;
}

css_error node_has_attribute_substring(void *pw, void *n,
		const css_qname *qname,
		lwc_string *value,
		bool *match)
{
	UNUSED(pw);
	UNUSED(n);
	UNUSED(qname);
	UNUSED(value);
	*match = false;
	return CSS_OK;
}

css_error node_is_first_child(void *pw, void *n, bool *match)
{
	UNUSED(pw);
	UNUSED(n);
	*match = false;
	return CSS_OK;
}

css_error node_is_root(void *pw, void *n, bool *match)
{
	UNUSED(pw);
	UNUSED(n);
	*match = false;
	return CSS_OK;
}

css_error node_count_siblings(void *pw, void *n,
		bool same_name, bool after, int32_t *count)
{
	UNUSED(pw);
	UNUSED(n);
	UNUSED(same_name);
	UNUSED(after);
	*count = 1;
	return CSS_OK;
}

css_error node_is_empty(void *pw, void *n, bool *match)
{
	UNUSED(pw);
	UNUSED(n);
	*match = false;
	return CSS_OK;
}

css_error node_is_link(void *pw, void *n, bool *match)
{
	// TODO how is that normally implemented
	UNUSED(pw);
	UNUSED(n);
	*match = false;
	return CSS_OK;
}

css_error node_is_visited(void *pw, void *n, bool *match)
{
	// TODO should we track visited links?
	UNUSED(pw);
	UNUSED(n);
	*match = false;
	return CSS_OK;
}

css_error node_is_hover(void *pw, void *n, bool *match)
{
	UNUSED(pw);
	UNUSED(n);
	*match = false;
	return CSS_OK;
}

css_error node_is_active(void *pw, void *n, bool *match)
{
	UNUSED(pw);
	UNUSED(n);
	*match = false;
	return CSS_OK;
}

css_error node_is_focus(void *pw, void *n, bool *match)
{
	UNUSED(pw);
	UNUSED(n);
	*match = false;
	return CSS_OK;
}

css_error node_is_enabled(void *pw, void *n, bool *match)
{
	UNUSED(pw);
	UNUSED(n);
	*match = false;
	return CSS_OK;
}

css_error node_is_disabled(void *pw, void *n, bool *match)
{
	UNUSED(pw);
	UNUSED(n);
	*match = false;
	return CSS_OK;
}

css_error node_is_checked(void *pw, void *n, bool *match)
{
	UNUSED(pw);
	UNUSED(n);
	*match = false;
	return CSS_OK;
}

css_error node_is_target(void *pw, void *n, bool *match)
{
	UNUSED(pw);
	UNUSED(n);
	*match = false;
	return CSS_OK;
}


css_error node_is_lang(void *pw, void *n,
		lwc_string *lang,
		bool *match)
{
	UNUSED(pw);
	UNUSED(n);
	UNUSED(lang);
	*match = false;
	return CSS_OK;
}

css_error node_presentational_hint(void *pw, void *node,
		uint32_t *nhints, css_hint **hints)
{
	UNUSED(pw);
	UNUSED(node);
	*nhints = 0;
	*hints = NULL;
	return CSS_OK;
}

css_error ua_default_for_property(void *pw, uint32_t property, css_hint *hint)
{
	UNUSED(pw);

	if (property == CSS_PROP_COLOR) {
		hint->data.color = 0x00000000;
		hint->status = CSS_COLOR_COLOR;
	} else if (property == CSS_PROP_FONT_FAMILY) {
		hint->data.strings = NULL;
		hint->status = CSS_FONT_FAMILY_SANS_SERIF;
	} else if (property == CSS_PROP_QUOTES) {
		/* Not exactly useful :) */
		hint->data.strings = NULL;
		hint->status = CSS_QUOTES_NONE;
	} else if (property == CSS_PROP_VOICE_FAMILY) {
		/** \todo Fix this when we have voice-family done */
		hint->data.strings = NULL;
		hint->status = 0;
	} else {
		return CSS_INVALID;
	}

	return CSS_OK;
}

// In theory libcss could need to store its own data in nodes, but we don't implement it.
// So these two functions just make sure there is no crash and no memory leak.
static css_error set_libcss_node_data(void *pw, void *n,
		void *libcss_node_data)
{
	UNUSED(pw);
	UNUSED(n);

	/* Since we're not storing it, ensure node data gets deleted */
	css_libcss_node_data_handler(&select_handler, CSS_NODE_DELETED,
			pw, n, NULL, libcss_node_data);

	return CSS_OK;
}

static css_error get_libcss_node_data(void *pw, void *n,
		void **libcss_node_data)
{
	UNUSED(pw);
	UNUSED(n);
	*libcss_node_data = NULL;

	return CSS_OK;
}


