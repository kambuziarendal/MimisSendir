// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include <Message.h>
#include "ui/textview/Paragraph.h"
inline bool IsReactionSpan(const TextSpan& span) {
    return span.ClickMessage() && (span.ClickMessage()->what=='rMnu' || span.ClickMessage()->what=='rBdg');
}
inline Paragraph WithoutReactions(const Paragraph& original) {
    Paragraph result(original.Style());
    for(int32 i=0;i<original.CountTextSpans();++i)
        if(!IsReactionSpan(original.TextSpanAtIndex(i)))result.Append(original.TextSpanAtIndex(i));
    return result;
}
inline void CopyReactions(const Paragraph& original,Paragraph& result) {
    auto style=result.Style();style.SetMessageContent(original.Style().MessageBody(),original.Style().MessageSender(),original.Style().MessageAuthor());style.SetAvatar(original.Style().Avatar(),original.Style().AvatarName());style.SetMessageTime(original.Style().MessageTime());style.SetDeliveryMark(original.Style().MessageDeliveryMark());style.SetReactionId(original.Style().ReactionId());style.SetOwnReaction(original.Style().OwnReaction(),original.Style().OwnReactionBitmap());style.SetReactionBadges(original.Style().ReactionBadges());result.SetStyle(style);
    for(int32 i=0;i<original.CountTextSpans();++i)
        if(IsReactionSpan(original.TextSpanAtIndex(i)))result.Append(original.TextSpanAtIndex(i));
}
