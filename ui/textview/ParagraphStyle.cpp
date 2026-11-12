/*
 * Copyright 2013, Stephan Aßmus <superstippi@gmx.de>.
 * All rights reserved. Distributed under the terms of the MIT License.
 */

#include "ParagraphStyle.h"


ParagraphStyle::ParagraphStyle()
	:
	fStyleData(new ParagraphStyleData(), true)
{
}


ParagraphStyle::ParagraphStyle(const ParagraphStyle& other)
	:
	fMessageKind(other.fMessageKind),
    fAvatar(other.fAvatar),fAvatarName(other.fAvatarName),
    fMessageTime(other.fMessageTime),
    fDateDivider(other.fDateDivider),
    fReactionId(other.fReactionId),
    fMessageBody(other.fMessageBody), fMessageSender(other.fMessageSender), fMessageAuthor(other.fMessageAuthor),
    fOwnReaction(other.fOwnReaction),
    fOwnReactionBitmap(other.fOwnReactionBitmap),
    fReactionBadges(other.fReactionBadges),
	fStyleData(other.fStyleData)
{
}


ParagraphStyle&
ParagraphStyle::operator=(const ParagraphStyle& other)
{
	if (this == &other)
		return *this;

	fMessageKind = other.fMessageKind;
    fAvatar=other.fAvatar;fAvatarName=other.fAvatarName;
    fMessageTime=other.fMessageTime;
    fDateDivider=other.fDateDivider;
    fReactionId = other.fReactionId;
    fMessageBody=other.fMessageBody;fMessageSender=other.fMessageSender;fMessageAuthor=other.fMessageAuthor;
    fOwnReaction = other.fOwnReaction;
    fOwnReactionBitmap = other.fOwnReactionBitmap;
    fReactionBadges=other.fReactionBadges;
	fStyleData = other.fStyleData;
	return *this;
}


bool
ParagraphStyle::operator==(const ParagraphStyle& other) const
{
	if (this == &other)
		return true;

	if(fMessageBody!=other.fMessageBody || fMessageSender!=other.fMessageSender || fMessageAuthor!=other.fMessageAuthor)return false;
	if(fReactionBadges!=other.fReactionBadges)return false;
	if(fAvatar!=other.fAvatar||fAvatarName!=other.fAvatarName)return false;
	if (fMessageTime != other.fMessageTime || fDateDivider != other.fDateDivider || fOwnReaction != other.fOwnReaction || fReactionId != other.fReactionId || fMessageKind != other.fMessageKind) return false;
	if (fStyleData == other.fStyleData)
		return true;

	if (fStyleData.IsSet() && other.fStyleData.IsSet())
		return *fStyleData == *other.fStyleData;

	return false;
}


bool
ParagraphStyle::operator!=(const ParagraphStyle& other) const
{
	return !(*this == other);
}


bool
ParagraphStyle::SetAlignment(::Alignment alignment)
{
	ParagraphStyleDataRef data = fStyleData->SetAlignment(alignment);
	if (data == fStyleData)
		return data->Alignment() == alignment;

	fStyleData = data;
	return true;
}


::Alignment
ParagraphStyle::Alignment() const
{
	return fStyleData->Alignment();
}


bool
ParagraphStyle::SetJustify(bool justify)
{
	ParagraphStyleDataRef data = fStyleData->SetJustify(justify);
	if (data == fStyleData)
		return data->Justify() == justify;

	fStyleData = data;
	return true;
}


bool
ParagraphStyle::Justify() const
{
	return fStyleData->Justify();
}


bool
ParagraphStyle::SetFirstLineInset(float inset)
{
	ParagraphStyleDataRef data = fStyleData->SetFirstLineInset(inset);
	if (data == fStyleData)
		return data->FirstLineInset() == inset;

	fStyleData = data;
	return true;
}


float
ParagraphStyle::FirstLineInset() const
{
	return fStyleData->FirstLineInset();
}


bool
ParagraphStyle::SetLineInset(float inset)
{
	ParagraphStyleDataRef data = fStyleData->SetLineInset(inset);
	if (data == fStyleData)
		return data->LineInset() == inset;

	fStyleData = data;
	return true;
}


float
ParagraphStyle::LineInset() const
{
	return fStyleData->LineInset();
}


bool
ParagraphStyle::SetLineSpacing(float spacing)
{
	ParagraphStyleDataRef data = fStyleData->SetLineSpacing(spacing);
	if (data == fStyleData)
		return data->LineSpacing() == spacing;

	fStyleData = data;
	return true;
}


float
ParagraphStyle::LineSpacing() const
{
	return fStyleData->LineSpacing();
}


bool
ParagraphStyle::SetSpacingTop(float spacing)
{
	ParagraphStyleDataRef data = fStyleData->SetSpacingTop(spacing);
	if (data == fStyleData)
		return data->SpacingTop() == spacing;

	fStyleData = data;
	return true;
}


float
ParagraphStyle::SpacingTop() const
{
	return fStyleData->SpacingTop();
}


bool
ParagraphStyle::SetSpacingBottom(float spacing)
{
	ParagraphStyleDataRef data = fStyleData->SetSpacingBottom(spacing);
	if (data == fStyleData)
		return data->SpacingBottom() == spacing;

	fStyleData = data;
	return true;
}


float
ParagraphStyle::SpacingBottom() const
{
	return fStyleData->SpacingBottom();
}


bool
ParagraphStyle::SetBullet(const ::Bullet& bullet)
{
	ParagraphStyleDataRef data = fStyleData->SetBullet(bullet);
	if (data == fStyleData)
		return data->Bullet() == bullet;

	fStyleData = data;
	return true;
}


const ::Bullet&
ParagraphStyle::Bullet() const
{
	return fStyleData->Bullet();
}


