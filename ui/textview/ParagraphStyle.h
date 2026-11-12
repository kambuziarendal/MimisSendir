/*
 * Copyright 2013, Stephan Aßmus <superstippi@gmx.de>.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef PARAGRAPH_STYLE_H
#define PARAGRAPH_STYLE_H

#include "ParagraphStyleData.h"
#include <string>
#include <ctime>
#include <memory>
#include <vector>
class BBitmap;
namespace Avatars { struct Image; }


class ParagraphStyle {
public:
    enum class DeliveryMark { None, Sending, Sent, Delivered, Read, Failed };
    struct ReactionBadge {
        std::string emoji;
        size_t count=1;
        std::shared_ptr<BBitmap> bitmap;
        bool operator==(const ReactionBadge& other) const { return emoji==other.emoji && count==other.count && bitmap==other.bitmap; }
    };
    void SetReactionBadges(std::vector<ReactionBadge> badges) { fReactionBadges=std::move(badges); }
    const std::vector<ReactionBadge>& ReactionBadges() const { return fReactionBadges; }
								ParagraphStyle();
								ParagraphStyle(const ParagraphStyle& other);

			ParagraphStyle&		operator=(const ParagraphStyle& other);
			bool				operator==(const ParagraphStyle& other) const;
			bool				operator!=(const ParagraphStyle& other) const;

			bool				SetAlignment(::Alignment alignment);
			::Alignment			Alignment() const;

			bool				SetJustify(bool justify);
			bool				Justify() const;

			bool				SetFirstLineInset(float inset);
			float				FirstLineInset() const;

			bool				SetLineInset(float inset);
			float				LineInset() const;

			bool				SetLineSpacing(float spacing);
			float				LineSpacing() const;

			bool				SetSpacingTop(float spacing);
			float				SpacingTop() const;

			bool				SetSpacingBottom(float spacing);
			float				SpacingBottom() const;

			bool				SetBullet(const ::Bullet& bullet);
			const ::Bullet&		Bullet() const;

    void SetAvatar(std::shared_ptr<Avatars::Image> image,const std::string& name){ fAvatar=std::move(image);fAvatarName=name; }
    const std::shared_ptr<Avatars::Image>& Avatar()const{return fAvatar;}
    const std::string& AvatarName()const{return fAvatarName;}
    void SetDeliveryMark(DeliveryMark value) { fDeliveryMark=value; }
    DeliveryMark MessageDeliveryMark() const { return fDeliveryMark; }
    void SetMessageTime(const std::string& value) { fMessageTime=value; }
    const std::string& MessageTime() const { return fMessageTime; }
    void SetDateDivider(time_t value) { fDateDivider=value; }
    time_t DateDivider() const { return fDateDivider; }
	void SetReactionId(const std::string& id) { fReactionId = id; }
    const std::string& ReactionId() const { return fReactionId; }
    void SetOwnReaction(const std::string& emoji, std::shared_ptr<BBitmap> bitmap = {}) { fOwnReaction = emoji; fOwnReactionBitmap = std::move(bitmap); }
    const std::string& OwnReaction() const { return fOwnReaction; }
    const std::shared_ptr<BBitmap>& OwnReactionBitmap() const { return fOwnReactionBitmap; }
	void SetMessageContent(const std::string& body, const std::string& sender, const std::string& name) { fMessageBody=body;fMessageSender=sender;fMessageAuthor=name; }
    const std::string& MessageBody() const { return fMessageBody; }
    const std::string& MessageSender() const { return fMessageSender; }
    const std::string& MessageAuthor() const { return fMessageAuthor; }
    void SetMessageKind(int kind) { fMessageKind = kind; }
	int MessageKind() const { return fMessageKind; }

private:
	int fMessageKind = 0;
    std::shared_ptr<Avatars::Image> fAvatar;
    std::string fAvatarName;
    std::string fMessageTime;
    DeliveryMark fDeliveryMark=DeliveryMark::None;
    time_t fDateDivider=-1;
    std::string fReactionId;
    std::string fMessageBody, fMessageSender, fMessageAuthor;
    std::string fOwnReaction;
    std::shared_ptr<BBitmap> fOwnReactionBitmap;
    std::vector<ReactionBadge> fReactionBadges;
			ParagraphStyleDataRef fStyleData;
};


#endif // PARAGRAPH_STYLE_H
