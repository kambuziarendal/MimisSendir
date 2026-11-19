/*
 * Copyright 2013-2015, Stephan Aßmus <superstippi@gmx.de>.
 * All rights reserved. Distributed under the terms of the MIT License.
 */

#include "TextDocumentLayout.h"

#include <algorithm>
#include <new>
#include <stdio.h>

#include <View.h>


class LayoutTextListener : public TextListener {
public:
	LayoutTextListener(TextDocumentLayout* layout)
		:
		fLayout(layout)
	{
	}

	virtual	void TextChanging(TextChangingEvent& event)
	{
		(void)event;
	}

	virtual	void TextChanged(const TextChangedEvent& event)
	{
		if (event.HasExactRange()) {
			fLayout->InvalidateParagraphs(event.FirstChangedParagraph(),
				event.ChangedParagraphCount());
		} else {
			// General text edits can insert or remove paragraphs.
			fLayout->Invalidate();
		}
	}

private:
	TextDocumentLayout*	fLayout;
};



TextDocumentLayout::TextDocumentLayout()
	:
	fWidth(0.0f),
	fFirstDirtyParagraph(0),

	fDocument(),
	fTextListener(new(std::nothrow) LayoutTextListener(this), true),
	fParagraphLayouts()
{
}


TextDocumentLayout::TextDocumentLayout(const TextDocumentRef& document)
	:
	fWidth(0.0f),
	fFirstDirtyParagraph(0),

	fDocument(),
	fTextListener(new(std::nothrow) LayoutTextListener(this), true),
	fParagraphLayouts()
{
	SetTextDocument(document);
}


TextDocumentLayout::TextDocumentLayout(const TextDocumentLayout& other)
	:
	TextDocumentLayout(other.fDocument)
{
	SetWidth(other.fWidth);
}


TextDocumentLayout::~TextDocumentLayout()
{
	SetTextDocument(NULL);
}


void
TextDocumentLayout::SetTextDocument(const TextDocumentRef& document)
{
	if (fDocument == document)
		return;

	if (fDocument.IsSet())
		fDocument->RemoveListener(fTextListener);

	fDocument = document;
	_Init();
	fFirstDirtyParagraph = 0;

	if (fDocument.IsSet())
		fDocument->AddListener(fTextListener);
}


void
TextDocumentLayout::Invalidate()
{
	if (fDocument.IsSet())
		InvalidateParagraphs(0, fDocument->CountParagraphs());
}


void
TextDocumentLayout::InvalidateParagraphs(int32 start, int32 count)
{
	if (start < 0 || !fDocument.IsSet())
		return;

	fFirstDirtyParagraph = std::min(fFirstDirtyParagraph, start);

	while (count > 0) {
		const int32 paragraphCount = fDocument->CountParagraphs();
		if (start >= paragraphCount)
			break;
		const Paragraph& paragraph = fDocument->ParagraphAtIndex(start);
		if (start >= static_cast<int32>(fParagraphLayouts.size())) {
			ParagraphLayoutRef layout(new(std::nothrow) ParagraphLayout(
				paragraph), true);
			if (!layout.IsSet()) {
				fprintf(stderr, "TextDocumentLayout::InvalidateParagraphs() - "
					"out of memory\n");
				return;
			}
			try {
				fParagraphLayouts.push_back(ParagraphLayoutInfo(0.0f, layout));
			}
			catch (std::bad_alloc& ba) {
				fprintf(stderr, "bad_alloc when invalidating paragraphs\n");
				return;
			}
		} else {
			const ParagraphLayoutInfo& info = fParagraphLayouts[start];
			info.layout->SetParagraph(paragraph);
		}

		start++;
		count--;
	}

	// Remove any extra paragraph layouts
	while (fDocument->CountParagraphs()
			< static_cast<int32>(fParagraphLayouts.size()))
		fParagraphLayouts.erase(fParagraphLayouts.end() - 1);
}


void
TextDocumentLayout::SetWidth(float width)
{
	if (fWidth != width) {
		fWidth = width;
		fFirstDirtyParagraph = 0;
	}
}


float
TextDocumentLayout::Height()
{
	_ValidateLayout();

	float height = 0.0f;

	if (fParagraphLayouts.size() > 0) {
		const ParagraphLayoutInfo& lastLayout
			= fParagraphLayouts[fParagraphLayouts.size() - 1];
		height = lastLayout.y + lastLayout.layout->Height();
	}

	return height;
}


void
TextDocumentLayout::Draw(BView* view, const BPoint& offset,
	const BRect& updateRect)
{
	_ValidateLayout();

	int layoutCount = fParagraphLayouts.size();
	for (int i = 0; i < layoutCount; i++) {
		const ParagraphLayoutInfo& layout = fParagraphLayouts[i];
		BPoint location(offset.x, offset.y + layout.y);
		if (location.y > updateRect.bottom)
			break;
		if (location.y + layout.layout->Height() > updateRect.top)
			layout.layout->Draw(view, location);
	}
}


int32
TextDocumentLayout::LineIndexForOffset(int32 textOffset)
{
	int32 index = _ParagraphLayoutIndexForOffset(textOffset);
	if (index >= 0) {
		int32 lineIndex = 0;
		for (int32 i = 0; i < index; i++) {
			lineIndex += fParagraphLayouts[i].layout->CountLines();
		}

		const ParagraphLayoutInfo& info = fParagraphLayouts[index];
		return lineIndex + info.layout->LineIndexForOffset(textOffset);
	}

	return 0;
}


int32
TextDocumentLayout::FirstOffsetOnLine(int32 lineIndex)
{
	int32 paragraphOffset;
	int32 index = _ParagraphLayoutIndexForLineIndex(lineIndex, paragraphOffset);
	if (index >= 0) {
		const ParagraphLayoutInfo& info = fParagraphLayouts[index];
		return info.layout->FirstOffsetOnLine(lineIndex) + paragraphOffset;
	}

	return 0;
}


int32
TextDocumentLayout::LastOffsetOnLine(int32 lineIndex)
{
	int32 paragraphOffset;
	int32 index = _ParagraphLayoutIndexForLineIndex(lineIndex, paragraphOffset);
	if (index >= 0) {
		const ParagraphLayoutInfo& info = fParagraphLayouts[index];
		return info.layout->LastOffsetOnLine(lineIndex) + paragraphOffset;
	}

	return 0;
}


int32
TextDocumentLayout::CountLines()
{
	_ValidateLayout();

	int32 lineCount = 0;

	int32 count = fParagraphLayouts.size();
	for (int32 i = 0; i < count; i++) {
		const ParagraphLayoutInfo& info = fParagraphLayouts[i];
		lineCount += info.layout->CountLines();
	}

	return lineCount;
}


void
TextDocumentLayout::GetLineBounds(int32 lineIndex, float& x1, float& y1,
	float& x2, float& y2)
{
	int32 paragraphOffset;
	int32 index = _ParagraphLayoutIndexForLineIndex(lineIndex, paragraphOffset);
	if (index >= 0) {
		const ParagraphLayoutInfo& info = fParagraphLayouts[index];
		info.layout->GetLineBounds(lineIndex, x1, y1, x2, y2);
		y1 += info.y;
		y2 += info.y;
		return;
	}

	x1 = 0.0f;
	y1 = 0.0f;
	x2 = 0.0f;
	y2 = 0.0f;
}


void
TextDocumentLayout::GetTextBounds(int32 textOffset, float& x1, float& y1,
	float& x2, float& y2)
{
	int32 index = _ParagraphLayoutIndexForOffset(textOffset);
	if (index >= 0) {
		const ParagraphLayoutInfo& info = fParagraphLayouts[index];
		info.layout->GetTextBounds(textOffset, x1, y1, x2, y2);
		y1 += info.y;
		y2 += info.y;
		return;
	}

	x1 = 0.0f;
	y1 = 0.0f;
	x2 = 0.0f;
	y2 = 0.0f;
}


int32
TextDocumentLayout::TextOffsetAt(float x, float y, bool& rightOfCenter)
{
	_ValidateLayout();

	int32 textOffset = 0;
	rightOfCenter = false;

	int32 paragraphs = fParagraphLayouts.size();
	for (int32 i = 0; i < paragraphs; i++) {
		const ParagraphLayoutInfo& info = fParagraphLayouts[i];
		if (y > info.y + info.layout->Height()) {
			textOffset += info.layout->CountGlyphs();
			continue;
		}

		textOffset += info.layout->TextOffsetAt(x, y - info.y, rightOfCenter);
		break;
	}

	return textOffset;
}

// #pragma mark - private


void
TextDocumentLayout::_ValidateLayout()
{
	const int32 count = fParagraphLayouts.size();
	if (fFirstDirtyParagraph >= count)
		return;
	_Layout();
	fFirstDirtyParagraph = count;
}


void
TextDocumentLayout::_Init()
{
	fParagraphLayouts.clear();

	if (!fDocument.IsSet())
		return;

	int paragraphCount = fDocument->CountParagraphs();
	for (int i = 0; i < paragraphCount; i++) {
		const Paragraph& paragraph = fDocument->ParagraphAtIndex(i);
		ParagraphLayoutRef layout(new(std::nothrow) ParagraphLayout(paragraph),
			true);
		if (!layout.IsSet()) {
			fprintf(stderr, "TextDocumentLayout::_Layout() - out of memory\n");
			return;
		}
		try {
			fParagraphLayouts.push_back(ParagraphLayoutInfo(0.0f, layout));
		}
		catch (std::bad_alloc& ba) {
			fprintf(stderr, "bad_alloc when inititalizing the text document "
				"layout\n");
			return;
		}
	}
}


void
TextDocumentLayout::_Layout()
{
	float y = 0.0f;
	if (fFirstDirtyParagraph > 0) {
		const auto& previous = fParagraphLayouts[fFirstDirtyParagraph - 1];
		y = previous.y;
		y += previous.layout->Height() + previous.layout->Style().SpacingBottom();
	}

	const int32 count = fParagraphLayouts.size();
	for (int32 i = fFirstDirtyParagraph; i < count; ++i) {
		auto& info = fParagraphLayouts[i];
		const ParagraphStyle& style = info.layout->Style();
		if (i > 0)
			y += style.SpacingTop();
		info.y = y;
		info.layout->SetWidth(fWidth);
		const float height = info.layout->Height();
		if (i > 0) {
			const auto& previous = fParagraphLayouts[i - 1];
			info.orderedBottoms = previous.orderedBottoms
				&& previous.y + previous.layout->Height() <= y + height;
		}
		y += height + style.SpacingBottom();
	}
}


int32
TextDocumentLayout::_ParagraphLayoutIndexForOffset(int32& textOffset)
{
	_ValidateLayout();

	int32 paragraphs = fParagraphLayouts.size();
	for (int32 i = 0; i < paragraphs - 1; i++) {
		const ParagraphLayoutInfo& info = fParagraphLayouts[i];

		int32 length = info.layout->CountGlyphs();
		if (textOffset >= length) {
			textOffset -= length;
			continue;
		}

		return i;
	}

	if (paragraphs > 0) {
		const ParagraphLayoutInfo& info
			= fParagraphLayouts[fParagraphLayouts.size() - 1];

		// Return last paragraph if the textOffset is still within or
		// exactly behind the last valid offset in that paragraph.
		int32 length = info.layout->CountGlyphs();
		if (textOffset <= length)
			return paragraphs - 1;
	}

	return -1;
}

int32
TextDocumentLayout::_ParagraphLayoutIndexForLineIndex(int32& lineIndex,
	int32& paragraphOffset)
{
	_ValidateLayout();

	paragraphOffset = 0;
	int32 paragraphs = fParagraphLayouts.size();
	for (int32 i = 0; i < paragraphs; i++) {
		const ParagraphLayoutInfo& info = fParagraphLayouts[i];

		int32 lineCount = info.layout->CountLines();
		if (lineIndex >= lineCount) {
			lineIndex -= lineCount;
			paragraphOffset += info.layout->CountGlyphs();
			continue;
		}

		return i;
	}

	return -1;
}

int32 TextDocumentLayout::MessageAt(float x,float y) {
    _ValidateLayout();
    for(size_t i=0;i<fParagraphLayouts.size();++i) {
        const auto& info=fParagraphLayouts[i];const auto& layout=info.layout;
        if(layout->Style().MessageKind() && layout->MessageBounds().Contains(BPoint(x,y-info.y)))return i;
    }
    return -1;
}
std::string TextDocumentLayout::ReactionAt(float x,float y) {
    _ValidateLayout();
    for(const auto& info:fParagraphLayouts)
        if(!info.layout->ReactionId().empty() && info.layout->ReactionButton().Contains(BPoint(x,y-info.y)))return info.layout->ReactionId();
    return {};
}
bool TextDocumentLayout::ExactTextHit(float x,float y,int32 offset) {
    float left,top,right,bottom;GetTextBounds(offset,left,top,right,bottom);
    return right>left && x>=left && x<right && y>=top && y<bottom;
}

BRect TextDocumentLayout::ParagraphBounds(int32 index)
{
    _ValidateLayout();
    if(index<0 || size_t(index)>=fParagraphLayouts.size())return BRect();
    const auto& info=fParagraphLayouts[index];
    return BRect(0,info.y,fWidth,info.y+info.layout->Height());
}

std::vector<int32>
TextDocumentLayout::ParagraphsEndingBetween(float top, float bottom)
{
	_ValidateLayout();
	std::vector<int32> result;
	if (fParagraphLayouts.empty())
		return result;

	// Negative paragraph spacing can overlap and reorder bottoms; in
	// that case retain the full scan instead of relying on the ordering.
	const bool ordered = fParagraphLayouts.back().orderedBottoms;
	const auto end = fParagraphLayouts.end();
	auto it = fParagraphLayouts.begin();
	if (ordered) {
		it = std::lower_bound(it, end, top,
			[](const auto& info, float y) { return info.y + info.layout->Height() < y; });
	}
	for (; it != end; ++it) {
		const float paragraphBottom = it->y + it->layout->Height();
		if (paragraphBottom > bottom && ordered)
			break;
		if (paragraphBottom >= top && paragraphBottom <= bottom)
			result.push_back(it - fParagraphLayouts.begin());
	}
	return result;
}
