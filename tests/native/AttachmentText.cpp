// Native Haiku test. Uses synthetic UI objects, no network/account data.
#include <Application.h>
#include <Window.h>
#include <ScrollView.h>
#include <ScrollBar.h>
#include <LayoutBuilder.h>
#include <Bitmap.h>
#include <Message.h>
#include <cstdio>
#include <cstdlib>
#include <atomic>
#include "ui/textview/Paragraph.h"
#include "ui/ReactionSpans.h"
#include "ui/AvatarCache.h"
#include "ui/textview/ParagraphLayout.h"
#include "ui/textview/TextDocument.h"
#include "ui/textview/TextDocumentView.h"

class ClickProbe final : public TextDocumentView {
public:
	uint32 clicked = 0;
	status_t Invoke(BMessage* message = nullptr) override {
		clicked = message ? message->what : 0;
		return B_OK;
	}
};

class RedrawProbe final : public TextDocumentView {
public:
    std::atomic<int> draws{0};
    void Draw(BRect update) override { ++draws; TextDocumentView::Draw(update); }
};

static int checks;
static void Check(bool result, const char* label)
{
	++checks;
	if (!result) { fprintf(stderr, "FAIL: %s\n", label); std::exit(1); }
}
int main()
{
	BApplication app("application/x-vnd.Kambuzi-RengaAttachmentTests");
	ParagraphStyle incoming; incoming.SetMessageKind(1);
	ParagraphStyle outgoing(incoming); outgoing.SetMessageKind(2);
	Check(incoming.MessageKind() == 1 && outgoing.MessageKind() == 2, "message direction copies independently");
	Check(incoming != outgoing, "message direction invalidates layout equality");
	incoming.SetMessageContent("Exact 🌞 body", "bob@fixture.test", "Bob");
    ParagraphStyle contentCopy(incoming);
    Check(contentCopy.MessageBody()=="Exact 🌞 body" && contentCopy.MessageSender()=="bob@fixture.test", "message menu payload survives style copy");
	ParagraphStyle assigned; assigned = outgoing;
	Check(assigned == outgoing, "message direction survives assignment");
	CharacterStyle style;
	TextSpan download("Download", style), save("Save", style);
	download.SetClickMessage(new BMessage('aGet'));
	save.SetClickMessage(new BMessage('aSav'));
	Paragraph actions;
	actions.Append(download); actions.Append(save);
	Check(actions.CountTextSpans() == 2, "same-style actions do not merge");
	Check(actions.TextSpanAtIndex(0).ClickMessage()->what == 'aGet', "download target retained");
	Check(actions.TextSpanAtIndex(1).ClickMessage()->what == 'aSav', "save target retained");
	TextSpan clipped = download.SubSpan(0, 4);
	Check(clipped.Text() == "Down", "subspan text preserved");
	Check(clipped.ClickMessage() && clipped.ClickMessage()->what == 'aGet', "wrapped action retains target");
    TextSpan quoted("Quoted text that wraps", style);
    quoted.SetQuote(true);
    Check(quoted.SubSpan(0, 6).IsQuote(), "wrapped quote retains block styling");
    TextSpan quoteCopy; quoteCopy = quoted;
    Check(quoteCopy.IsQuote() && quoteCopy == quoted, "quote assignment retains styling");
    TextSpan notQuoted(quoted); notQuoted.SetQuote(false);
    Check(notQuoted != quoted && !notQuoted.CanMergePlain(quoted), "quote cannot merge into ordinary body");
    Paragraph quoteBlock;
    quoteBlock.Append(quoted); quoteBlock.Append(quoted);
    Check(quoteBlock.CountTextSpans() == 1 && quoteBlock.TextSpanAtIndex(0).IsQuote(), "merged quote lines retain styling");
    quoteBlock.Prepend(quoted);
    Check(quoteBlock.TextSpanAtIndex(0).IsQuote(), "prepended quote retains styling");
    TextSpan reaction("\n👍 2 + React",style);reaction.SetClickMessage(new BMessage('rMnu'));
    Paragraph withReaction;withReaction.Append(TextSpan("Document",style));withReaction.Append(reaction);
    auto card=WithoutReactions(withReaction);card.Append(TextSpan("\nPDF document",style));CopyReactions(withReaction,card);
    Check(card.Text()=="Document\nPDF document\n👍 2 + React","attachment render keeps reaction footer once");
    auto changed=WithoutReactions(card);changed.Append(reaction);
    Check(changed.Text()==card.Text(),"reaction redraw preserves attachment body");
    Check(IsReactionSpan(reaction.SubSpan(1,2)),"wrapped reaction retains action marker");
	auto bitmap = std::make_shared<BBitmap>(BRect(0, 0, 3, 2), B_RGBA32);
	Check(bitmap->InitCheck() == B_OK, "native bitmap allocation");
	TextSpan image; image.SetBitmap(bitmap);
	Check(image.CountChars() == 1, "inline image occupies one object character");
	Check(image.Style().Width() == 4 && image.Style().Ascent() == 3, "inline image layout dimensions");
	Check(image.SubSpan(0, 1).Bitmap() == bitmap, "layout subspan retains image ownership");
    image.SetVideoOverlay(1);
    Check(image.SubSpan(0,1).VideoOverlay()==1, "video play overlay survives wrapping");
    TextSpan playing(image); playing.SetVideoOverlay(0);
    Check(playing!=image, "pause/play overlay change invalidates span");
    playing=image;
    Check(playing.VideoOverlay()==1, "video overlay copied with thumbnail");
	TextSpan copied(image);
	Check(copied == image, "image copy equality");
	Paragraph plain;
	plain.Append(TextSpan("one", style)); plain.Append(TextSpan("two", style));
	Check(plain.CountTextSpans() == 1 && plain.Text() == "onetwo", "ordinary text still merges");
	TextDocument document;
	document.Append(plain); document.Append(actions);
	Paragraph preview; preview.Append(image);
	Check(document.SetParagraph(0, preview), "replace original attachment paragraph");
	Check(document.CountParagraphs() == 2, "replacement keeps message indices stable");
	Check(document.ParagraphAtIndex(1).TextSpanAtIndex(0).ClickMessage()->what == 'aGet', "later message association unchanged");
	Check(!document.SetParagraph(-1, preview) && !document.SetParagraph(2, preview), "invalid paragraph indices rejected");
	Paragraph bubble(outgoing);
	bubble.Append(TextSpan("Synthetic link with a deliberately long line that wraps in a narrow conversation view", style));
	ParagraphLayout layout(bubble);
	for (float width : {260.0f, 900.0f, 260.0f}) {
		layout.SetWidth(width);
		float x1, y1, x2, y2;
		layout.GetTextBounds(0, x1, y1, x2, y2);
		Check(x1 >= width * 0.18f && x2 < width, "outgoing text stays inside bounded right bubble");
		bool right = false;
		Check(layout.TextOffsetAt(x1 + 0.1f, (y1+y2)/2, right) == 0, "bubble hit testing uses rendered coordinates");
	}
	Check(layout.CountLines() > 1, "narrow bubble wraps long text");
	for (int direction : {1, 2}) {
		ParagraphStyle messageStyle; messageStyle.SetMessageKind(direction);
		Paragraph card(messageStyle);
		card.Append(TextSpan("Caption\n", style)); card.Append(image);
		card.Append(TextSpan("\n", style)); card.Append(save);
		TextDocumentRef cards(new TextDocument(), true); cards->Append(card);
		ClickProbe view; view.SetTextDocument(cards); view.SetInsets(12);
		view.SetSelectionEnabled(false);
		for (float width : {260.0f, 900.0f, 260.0f}) {
			view.FrameResized(width, 300);
			ParagraphLayout expected(card); expected.SetWidth(width - 24);
			float x1, y1, x2, y2;
			expected.GetTextBounds(10, x1, y1, x2, y2);
			view.clicked = 0;
			view.MouseDown(BPoint(12 + (x1 + x2) / 2, 12 + (y1 + y2) / 2));
			Check(view.clicked == 'aSav', "native inset attachment action survives bubble resize");
		}
	}
    for(int direction:{1,2}) {
        ParagraphStyle bubble; bubble.SetMessageKind(direction);bubble.SetReactionId("message-42");bubble.SetOwnReaction("👍");
        Paragraph p(bubble);p.Append(TextSpan("A message",style));
        TextDocumentRef doc(new TextDocument(),true);doc->Append(p);doc->Append(p);
        ClickProbe view;view.SetTextDocument(doc);view.SetInsets(12);view.SetSelectionEnabled(false);
        for(float width:{260.0f,900.0f}) {
            view.FrameResized(width,300);ParagraphLayout expected(p);expected.SetWidth(width-24);
            auto button=expected.ReactionButton();
            view.clicked=0;view.MouseDown(BPoint(12+(button.left+button.right)/2,12+(button.top+button.bottom)/2));
            Check(view.clicked=='rMnu',"only dedicated corner icon opens reactions");
            for(BPoint point:{BPoint(width-3,12+button.top),BPoint(1,12+button.top),BPoint(12+button.left-5,12+button.top),BPoint(30,700)}) {
                view.clicked=0;view.MouseDown(point);Check(view.clicked==0,"blank space never opens reactions");
            }
        }
        Paragraph clean=WithoutReactions(p);Paragraph rebuilt;CopyReactions(p,rebuilt);
        Check(clean.Style().ReactionId()=="message-42" && rebuilt.Style().ReactionId()=="message-42","reaction corner survives attachment rebuild");
    }
    auto avatar=std::make_shared<Avatars::Image>();
    for(int direction:{1,2}) {
        ParagraphStyle grouped;grouped.SetMessageKind(direction);grouped.SetReactionId("group");grouped.SetOwnReaction("👍");grouped.SetAvatar(avatar,"Bob");
        Paragraph p(grouped);p.Append(TextSpan("Group content",style));
        Paragraph rebuilt;CopyReactions(p,rebuilt);
        Check(rebuilt.Style().Avatar()==avatar&&rebuilt.Style().AvatarName()=="Bob","attachment redraw retains participant identity slot");
        ParagraphStyle copied=grouped;Check(copied==grouped,"group avatar survives style copy");copied.SetAvatar({},"");Check(copied!=grouped,"avatar removal invalidates layout");
        for(float width:{180.f,260.f,900.f}) {
            ParagraphLayout layout(p);layout.SetWidth(width);float x1,y1,x2,y2;layout.GetTextBounds(0,x1,y1,x2,y2);
            Check(x1>=(direction==1?48:10)&&x2<width-(direction==2?38:0),"group content reserves avatar gutter on correct side");
            TextDocumentRef doc(new TextDocument(),true);doc->Append(p);ClickProbe view;view.SetTextDocument(doc);view.SetInsets(0);view.SetSelectionEnabled(false);view.FrameResized(width,300);
            view.clicked=0;view.MouseDown(BPoint(direction==1?15:width-15,15));Check(view.clicked==0,"participant avatar does not open reaction menu");
            auto button=layout.ReactionButton();view.MouseDown(BPoint((button.left+button.right)/2,(button.top+button.bottom)/2));Check(view.clicked=='rMnu',"group reaction target follows avatar gutter");
        }
    }
    ParagraphStyle selected;selected.SetMessageKind(1);selected.SetMessageTime("14:25");selected.SetReactionId("corner");selected.SetOwnReaction("❤️",bitmap);
    Paragraph original(selected);original.Append(TextSpan("Photo",style));
    Paragraph replacement;CopyReactions(original,replacement);
    Check(replacement.Style().MessageTime()=="14:25","clock survives asynchronous attachment rebuild");
    Check(replacement.Style().OwnReaction()=="❤️"&&replacement.Style().OwnReactionBitmap()==bitmap,"attachment redraw retains selected corner emoji");
    ParagraphStyle cleared=selected;cleared.SetOwnReaction("");
    Paragraph noReaction(cleared);noReaction.Append(TextSpan("No placeholder",style));ParagraphLayout emptyCorner(noReaction);emptyCorner.SetWidth(400);
    Check(!emptyCorner.ReactionButton().IsValid(),"no empty reaction placeholder or hit target");
    Check(cleared!=selected&&!cleared.OwnReactionBitmap(),"removal invalidates corner rendering and releases bitmap");
    for(int direction:{1,2}) {
        ParagraphStyle imageStyle;imageStyle.SetMessageKind(direction);imageStyle.SetReactionId("photo");imageStyle.SetOwnReaction("❤️",bitmap);
        auto large=std::make_shared<BBitmap>(BRect(0,0,1023,575),B_RGBA32);
        TextSpan image;image.SetBitmap(large);image.SetClickMessage(new BMessage('aMnu'));
        Paragraph photo(imageStyle);photo.Append(image);TextDocumentRef doc(new TextDocument(),true);doc->Append(photo);
        ClickProbe view;view.SetTextDocument(doc);view.SetInsets(12);view.SetSelectionEnabled(false);
        for(float width:{260.0f,900.0f,260.0f}) {
            view.FrameResized(width,700);ParagraphLayout layout(photo);layout.SetWidth(width-24);
            float x1,y1,x2,y2;layout.GetTextBounds(0,x1,y1,x2,y2);
            Check(x2<=width-24&&y2-y1<481,"high-resolution image fits narrow and wide bubble without excess height");
            view.clicked=0;view.MouseDown(BPoint(12+(x1+x2)/2,12+(y1+y2)/2));Check(view.clicked=='aMnu',"resized image remains clickable");
            auto target=layout.ReactionButton();view.clicked=0;view.MouseDown(BPoint(12+(target.left+target.right)/2,12+(target.top+target.bottom)/2));
            Check(view.clicked=='rMnu',"selected corner emoji retains reaction target after resizing");
        }
    }
    for(int direction:{1,2})for(float width:{180.f,260.f,900.f}) for(auto mark:{ParagraphStyle::DeliveryMark::None, ParagraphStyle::DeliveryMark::Sent, ParagraphStyle::DeliveryMark::Delivered, ParagraphStyle::DeliveryMark::Read}) {
        ParagraphStyle footer;footer.SetDeliveryMark(mark);footer.SetMessageKind(direction);footer.SetReactionId("footer-message");footer.SetMessageTime("23:12 · edited, unconfirmed");
        footer.SetReactionBadges({{"❤️",2,bitmap},{"👍",128,bitmap},{"😂",1,bitmap},{"🌞",4,bitmap}});
        Paragraph p(footer);p.Append(TextSpan("Footer remains below content",style));
        ParagraphLayout layout(p);layout.SetWidth(width);auto reactions=layout.ReactionButton(),clock=layout.TimeBounds(),bubble=layout.MessageBounds();
        Check(reactions.IsValid() && reactions.left>=bubble.left && reactions.right<=bubble.right,"aggregate reaction footer stays inside narrow and wide bubbles");
        Check(!reactions.Intersects(clock) && clock.right==bubble.right-8,"reactions do not overlap right-aligned clock or edited status");
        float x1,y1,x2,y2;layout.GetTextBounds(p.Length()-1,x1,y1,x2,y2);
        Check(reactions.top>=y2 && clock.top>=y2,"reaction and clock hit regions never cover message text");
        Paragraph copy;CopyReactions(p,copy);Check(copy.Style().ReactionBadges()==footer.ReactionBadges(),"attachment redraw retains all reaction counts");
        Check(copy.Style().MessageDeliveryMark()==mark,"attachment redraw retains sent/delivered/read marker");
        ParagraphStyle assigned;assigned=footer;Check(assigned==footer,"delivery marker survives style assignment");
        auto changed=footer;changed.SetDeliveryMark(ParagraphStyle::DeliveryMark::Failed);Check(changed!=footer,"delivery change invalidates cached paragraph style");
        TextDocumentRef doc(new TextDocument(),true);doc->Append(p);ClickProbe view;view.SetTextDocument(doc);view.SetInsets(0);view.SetSelectionEnabled(false);view.FrameResized(width,500);
        view.clicked=0;view.MouseDown(BPoint((reactions.left+reactions.right)/2,(reactions.top+reactions.bottom)/2));Check(view.clicked=='rMnu',"aggregate footer opens original message reaction menu");
        view.clicked=0;view.MouseDown(BPoint((clock.left+clock.right)/2,(clock.top+clock.bottom)/2));Check(view.clicked==0,"clock is not a reaction target");
    }
    {
        auto* window = new BWindow(BRect(40,40,500,400), "Read visibility", B_TITLED_WINDOW, B_ASYNCHRONOUS_CONTROLS);
        auto* view = new TextDocumentView("read-visibility");
        TextDocumentRef doc(new TextDocument(),true);
        Paragraph p; p.Append(TextSpan("Visible incoming message",style)); doc->Append(p);
        for(int i=0;i<40;++i) doc->Append(p);
        view->SetTextDocument(doc);view->SetInsets(12);
        auto* scroller = new BScrollView("read-scroll",view,0,false,true);
        BLayoutBuilder::Group<>(window,B_VERTICAL,0).Add(scroller);
        window->Show();snooze(150000);window->Lock();
        view->Relayout();
        Check(view->ParagraphReadVisible(0),"fully visible first message can be read");
        Check(!view->ParagraphReadVisible(40),"offscreen message is not read-visible");
        auto* bar=scroller->ScrollBar(B_VERTICAL);
        bar->SetValue(15);
        Check(!view->ParagraphReadVisible(0),"partly clipped message is not read-visible");
        float low,high;bar->GetRange(&low,&high);bar->SetValue(high);
        Check(view->ParagraphReadVisible(40),"bottom jump reveals newest message for reading");
        view->Hide();Check(!view->ParagraphReadVisible(40),"hidden timeline cannot mark read");
        window->Quit();
    }
    {
        auto* window = new BWindow(BRect(50,50,550,350), "Caret redraw regression", B_TITLED_WINDOW, B_ASYNCHRONOUS_CONTROLS);
        auto* view = new RedrawProbe();
        TextDocumentRef doc(new TextDocument(),true);
        Paragraph p; p.Append(TextSpan("Readonly conversation selection",style));doc->Append(p);
        view->SetTextDocument(doc);view->SetEditingEnabled(false);
        BLayoutBuilder::Group<>(window,B_VERTICAL,0).Add(view);
        window->Show();snooze(200000);window->Lock();
        view->MakeFocus(true);view->SetCaret(BPoint(20,10),false);
        window->Unlock();snooze(200000);view->draws=0;snooze(1200000);
        Check(view->draws.load()==0,"readonly timeline stays idle after clicking text");
        window->Lock();view->SelectAll();window->Unlock();
        snooze(200000);view->draws=0;snooze(1200000);
        Check(view->draws.load()==0,"readonly selection does not start a caret repaint timer");
        window->Lock();view->SetEditingEnabled(true);view->SetCaret(BPoint(20,10),false);window->Unlock();
        snooze(200000);view->draws=0;snooze(1200000);
        Check(view->draws.load()>0,"editable focused text retains blinking caret");
        window->Lock();view->SetEditingEnabled(false);window->Unlock();
        snooze(200000);view->draws=0;snooze(1200000);
        Check(view->draws.load()==0,"disabling editing cancels pending caret timer");
        window->Lock();window->Quit();
    }
	printf("%d native attachment text checks passed\n", checks);
}
