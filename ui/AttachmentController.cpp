// Copyright 2026 Kambuzi. Distributed under the MIT license.
#include "AttachmentController.h"
#include "AttachmentSave.h"
#include "ReactionSpans.h"
#include "MediaPreview.h"
#include "VideoPlayback.h"
#include <PopUpMenu.h>
#include <MenuItem.h>
#include "network/AttachmentMetadata.h"
#include "network/HttpTransfer.h"
#include "support/ImageSafety.h"
#include <Bitmap.h>
#include <DataIO.h>
#include <Directory.h>
#include <File.h>
#include <FindDirectory.h>
#include <Path.h>
#include <Entry.h>
#include <Roster.h>
#include <OS.h>
#include <ScrollBar.h>
#include <TranslationUtils.h>
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <thread>
#include <stdexcept>
#include <unistd.h>

namespace {
constexpr uint32 kPoll = 'aPls', kGet = 'aGet', kSave = 'aSav', kCancel = 'aCan', kSaveRef = 'aRef', kMenu = 'aMnu', kPlay = 'aPly', kVideo = 'aVid', kExternal = 'aExt', kSeek = 'aSek', kMute = 'aMut', kStop = 'aStp';
std::atomic<unsigned> sWorkers{0};
std::atomic<unsigned> sCachedFiles{0};
std::atomic<uint64> sNextToken{1};
bool Reserve(std::atomic<unsigned>& count, unsigned maximum)
{
	unsigned value = count.load();
	while (value < maximum) {
		if (count.compare_exchange_weak(value, value + 1)) return true;
	}
	return false;
}
struct WorkerLease { ~WorkerLease() { --sWorkers; } };


}

struct AttachmentController::Job {
	uint64 token = sNextToken.fetch_add(1);
	std::string url, filename, temporaryDirectory, temporaryPath, error;
	std::shared_ptr<FILE> file;
	MediaPreview preview;
	std::atomic_bool running{false}, cancelled{false}, saving{false};
	std::atomic<uint64> bytes{0}, total{0};
    std::atomic_bool playing{false};
	bool ready = false, saved = false, cacheLease = false;
	void ClearCache() {
		file.reset();
		preview = {};
		if (!temporaryPath.empty()) unlink(temporaryPath.c_str());
		if (!temporaryDirectory.empty()) rmdir(temporaryDirectory.c_str());
		temporaryPath.clear(); temporaryDirectory.clear();
		if (cacheLease) { --sCachedFiles; cacheLease = false; }
	}
	~Job() { if (!playing) ClearCache(); else if (cacheLease) --sCachedFiles; }
};
struct AttachmentController::Record {
	std::shared_ptr<Job> job;
	int32 paragraph;
	Paragraph prefix;
	bool wantsSave = false;
    bool wantsPlay = false;
	std::string status;
	std::shared_ptr<BBitmap> displayed;
	size_t frame = 0;
	bigtime_t nextFrame = 0;
	bool paused = false;
    bool automatic = false;
    BSize previewSize;
    std::unique_ptr<VideoPlayback> video;
    VideoPlayback::Snapshot videoState;
    uint64 videoGeneration = 0;
};

AttachmentController::AttachmentController(BMessenger target, TextDocumentView& timeline,
	const TextDocumentRef& document, std::string authority, std::string caFile)
	: fTarget(target), fTimeline(timeline), fDocument(document), fAuthority(std::move(authority)), fCAFile(std::move(caFile))
{
	BMessage tick(kPoll);
	fTimer = new BMessageRunner(target, tick, 50000);
}
AttachmentController::~AttachmentController()
{
	delete fTimer;
	delete fSavePanel;
	for (const auto& record : fRecords) record->job->cancelled = true;
}

void AttachmentController::Rebind(const TextDocumentRef& document)
{
    fDocument=document;
    for(auto& record:fRecords)record->paragraph=-1;
}

void AttachmentController::Add(const std::string& url)
{
	if (!Upload::IsServiceUrl(url, fAuthority)) return;
    // Reuse the downloaded bitmap, animation and in-flight save/download job
    // when history insertion moves this attachment to a different paragraph.
    for(auto& record:fRecords)if(record->paragraph<0 && record->job->url==url) {
        record->paragraph=fDocument->CountParagraphs()-1;
        record->prefix=fDocument->ParagraphAtIndex(record->paragraph);
        Render(*record);return;
    }
	// Bound retained metadata and inline bitmaps per chat. Eviction keeps a URL,
	// not a dangling action token, and drops the cached file when its worker exits.
	if (fRecords.size() >= 1000) {
		auto& oldest = *fRecords.front();
		oldest.job->cancelled = true;
		Paragraph expired(WithoutReactions(oldest.prefix));
		expired.Append(TextSpan("\n", CharacterStyle()));
		TextSpan link((oldest.job->filename + " (open attachment)").c_str(), CharacterStyle());
		auto* message = new BMessage('hope');
		message->AddString("url", oldest.job->url.c_str());
		link.SetClickMessage(message);
		expired.Append(link);
        if(oldest.paragraph>=0 && oldest.paragraph<fDocument->CountParagraphs()) {
            CopyReactions(fDocument->ParagraphAtIndex(oldest.paragraph),expired);
            fDocument->SetParagraph(oldest.paragraph, expired);
        }
		fRecords.erase(fRecords.begin());
	}
	auto record = std::make_unique<Record>();
	record->job = std::make_shared<Job>();
	record->job->url = url;
	record->job->filename = Upload::AttachmentFilename(url);
	if (fDocument->CountParagraphs() == 0) fDocument->Append(Paragraph());
	record->paragraph = fDocument->CountParagraphs() - 1;
	record->prefix = fDocument->ParagraphAtIndex(record->paragraph);
    record->automatic=Upload::IsPreviewFilename(url);
    record->status=record->automatic?"Loading image…":"Attachment. Download or save to view.";
	Render(*record);
	fRecords.push_back(std::move(record));
}

void AttachmentController::Render(Record& record)
{
    if(record.paragraph<0 || record.paragraph>=fDocument->CountParagraphs())return;
	BScrollBar* scroll = fTimeline.ScrollBar(B_VERTICAL);
	float minimum = 0, maximum = 0;
	if (scroll) scroll->GetRange(&minimum, &maximum);
	const bool atBottom = scroll && scroll->Value() >= maximum - 1;
	Paragraph paragraph(WithoutReactions(record.prefix));
	paragraph.Append(TextSpan("\n", CharacterStyle()));
	CharacterStyle normal;
	normal.SetForegroundColor(B_DOCUMENT_TEXT_COLOR);
    auto* options = new BMessage(kMenu);
    options->AddUInt64("attachment-token", record.job->token);
    TextSpan content;
    if (record.displayed) content.SetBitmap(record.displayed);
    else if(record.automatic && record.previewSize.width>0 && record.previewSize.height>0) {
        // Preserve the image's layout while its offscreen cache is recycled.
        CharacterStyle placeholder(normal);placeholder.SetWidth(record.previewSize.width);
        placeholder.SetAscent(record.previewSize.height);placeholder.SetDescent(0);
        content=TextSpan(" ",placeholder);
    } else {
        std::string label = Upload::IsPreviewFilename(record.job->url)
            ? (record.status.empty() ? "Image. Click for options." : record.status)
            : Upload::AttachmentDisplayName(record.job->url)
                + "\n" + (!record.status.empty() ? record.status : (!record.job->running && record.job->ready
                    ? (record.job->bytes.load()<1024 ? std::to_string(record.job->bytes.load()) + " bytes" : std::to_string(record.job->bytes.load()/1024) + " KiB") + " · Save or open…"
                    : "Save or open…"));
        if(!Upload::IsPreviewFilename(record.job->url)) {
            CharacterStyle heading(normal);heading.SetBold(true);
            TextSpan title((Upload::AttachmentType(record.job->filename)+"\n").c_str(),heading);
            title.SetClickMessage(new BMessage(*options));title.SetCursor(BCursor(B_CURSOR_ID_FOLLOW_LINK));
            paragraph.Append(title);
        }
        content = TextSpan(label.c_str(), normal);
    }
    content.SetClickMessage(options);
    content.SetCursor(BCursor(B_CURSOR_ID_FOLLOW_LINK));
    paragraph.Append(content);
    if (Upload::IsVideoFilename(record.job->url)) {
        auto control = [&](const std::string& label, uint32 code, int64 seek = 0) {
            TextSpan span(label.c_str(), normal);
            auto* action = new BMessage(code);
            action->AddUInt64("attachment-token", record.job->token);
            action->AddInt64("seek", seek);
            span.SetClickMessage(action);
            span.SetCursor(BCursor(B_CURSOR_ID_FOLLOW_LINK));
            paragraph.Append(span);
        };
        if (record.video) {
            const auto& state = record.videoState;
            auto time = [](bigtime_t value) {
                const auto seconds = std::max<int64>(0, value / 1000000);
                char text[40]; snprintf(text, sizeof(text), "%lld:%02lld",
                    (long long)(seconds / 60), (long long)(seconds % 60));
                return std::string(text);
            };
            paragraph.Append(TextSpan(("\n" + time(state.position) + " / " + time(state.duration)).c_str(), normal));
            control(state.ended ? "\n▶ Replay" : state.paused ? "\n▶ Play" : "\nⅡ Pause", kVideo);
            control("   −10 s", kSeek, -10000000);
            control("   +10 s", kSeek, 10000000);
            if (state.audio) control(state.muted ? "   Unmute" : "   Mute", kMute);
            control("   Close", kStop);
            if (!state.error.empty()) paragraph.Append(TextSpan(("\n" + state.error).c_str(), normal));
        } else control(record.job->running ? "\nCancel download" : "\n▶ Play video",
            record.job->running ? kCancel : kVideo);
    }
    CopyReactions(fDocument->ParagraphAtIndex(record.paragraph),paragraph);
	fDocument->SetParagraph(record.paragraph, paragraph);
	fTimeline.Relayout();
	if (atBottom) {
		scroll->GetRange(&minimum, &maximum);
		scroll->SetValue(maximum);
	}
	fTimeline.Invalidate();
}

void AttachmentController::Start(Record& record)
{
	auto job = record.job;
	if (job->running || job->ready) return;
	if (!Reserve(sWorkers, 2)) { record.status = "Two transfers are active. Try Download again shortly."; return; }
	if (!job->cacheLease && !Reserve(sCachedFiles, 8)) {
		--sWorkers;
		record.status = "Attachment cache is full. Close an old chat, then retry.";
		return;
	}
	record.automatic = false;
	job->cacheLease = true;
	job->cancelled = false;
	job->running = true;
	job->saving = false;
	job->bytes = 0;
	job->total = 0;
	job->error.clear();
	record.status = "Downloading…";
	try { std::thread(Download, job, fAuthority, fCAFile).detach(); }
	catch (const std::system_error&) {
		--sWorkers;
		job->ClearCache();
		job->error = "Could not start download worker.";
		job->running = false;
	}
}

void AttachmentController::Download(std::shared_ptr<Job> job, std::string authority, std::string caFile)
{
	WorkerLease worker;
	try {
		if (!job->file) {
			BPath directory;
			if (find_directory(B_SYSTEM_TEMP_DIRECTORY, &directory) != B_OK)
				throw std::runtime_error("Could not locate temporary directory.");
			std::string pattern = std::string(directory.Path()) + "/mimissendir-attachment-XXXXXX";
			std::vector<char> name(pattern.begin(), pattern.end()); name.push_back(0);
			if (!mkdtemp(name.data())) throw std::runtime_error("Could not create private attachment directory.");
			job->temporaryDirectory = name.data();
			job->temporaryPath = job->temporaryDirectory + "/" + job->filename;
			int fd = open(job->temporaryPath.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600);
			if (fd < 0) throw std::runtime_error("Could not create private attachment file.");
			FILE* file = fdopen(fd, "w+b");
			if (!file) { close(fd); throw std::runtime_error("Could not open private attachment file."); }
			job->file = std::shared_ptr<FILE>(file, fclose);
		}
		rewind(job->file.get());
		if (ftruncate(fileno(job->file.get()), 0) != 0)
			throw std::runtime_error("Could not reset failed attachment download.");
		auto result = Upload::GetFile(job->url, authority, job->file.get(), job->cancelled,
			[job](uint64_t done, uint64_t total) { job->bytes = done; job->total = total; }, caFile);
		job->ready = result.success;
		job->error = result.error;
		if (result.success && !job->cancelled) job->preview = LoadMediaPreview(job->file.get(),true);
		if (job->cancelled) { job->ready = false; job->error = "Download cancelled."; }
	} catch (const std::exception&) {
		job->ready = false;
		job->error = "Could not download or prepare attachment. Retry or use Open URL.";
	}
	if (!job->ready) job->ClearCache();
	job->running = false;
}

void AttachmentController::PlayVideo(Record& record)
{
    auto job = record.job;
    if (!job->ready || job->running || !Upload::IsVideoFilename(job->url)) return;
    job->error.clear();
    if (record.video && !record.videoState.stopped) {
        if (record.videoState.ended) {
            record.video->Seek(0);
            record.video->Pause(false);
        } else record.video->Pause(!record.videoState.paused);
        return;
    }
    record.video.reset();
    record.displayed.reset();
    record.videoGeneration = 0;
    record.videoState = {};
    record.status = "Opening video…";
    record.video = std::make_unique<VideoPlayback>(job->temporaryPath, job);
}

void AttachmentController::OpenExternalVideo(Record& record)
{
    auto job = record.job;
    if (!job->ready || job->running || !Upload::IsVideoFilename(job->url)) return;
    entry_ref ref;
    if (get_ref_for_path(job->temporaryPath.c_str(), &ref) != B_OK) {
        job->error = "Downloaded video is unavailable. Download it again.";
        return;
    }
    job->error.clear();
    BMessage open(B_REFS_RECEIVED);
    open.AddRef("refs", &ref);
    team_id player = -1;
    const auto result = be_roster->Launch("application/x-vnd.Haiku-MediaPlayer", &open, &player);
    if (result != B_OK && result != B_ALREADY_RUNNING) {
        job->error = "Could not open MediaPlayer. Save the video and open it manually.";
        return;
    }
    if (player < 0) player = be_roster->TeamFor("application/x-vnd.Haiku-MediaPlayer");
    if (!job->playing.exchange(true)) {
        // The player may keep using the file after its chat closes. Hold the
        // private cache until the player exits, without touching any UI object.
        try { std::thread([job, player] {
            team_info info;
            while (player >= 0 && get_team_info(player, &info) == B_OK) snooze(1000000);
            job->playing = false;
        }).detach(); }
        catch (const std::system_error&) {
            // Preserve the file rather than racing an already launched player.
            job->error = "Video opened. Its temporary file is retained until restart.";
        }
    }
}

void AttachmentController::SavePanel(Record& record)
{
	if (fSavePanel && fSavePanel->IsShowing()) {
		record.status = "Finish the current save dialog first.";
		return;
	}
	delete fSavePanel;
	BMessage selected(kSaveRef);
	selected.AddUInt64("attachment-token", record.job->token);
	fSavePanel = new BFilePanel(B_SAVE_PANEL, &fTarget, nullptr, B_FILE_NODE, false, &selected);
	fSavePanel->SetSaveText(record.job->filename.c_str());
	fSavingToken = record.job->token;
	fSavePanel->Show();
}

void AttachmentController::Save(std::shared_ptr<Job> job, std::string directory, std::string name)
{
	WorkerLease worker;
	job->error = Upload::SaveAttachmentExclusive(job->file.get(), directory + "/" + name, job->cancelled);
	job->saved = job->error.empty();
	job->running = false;
}

void AttachmentController::Poll()
{
    // Keep eight full media caches and two workers globally, but never make the
    // ninth image require a click. Visible images get priority; idle offscreen
    // caches can be reclaimed without changing bubble geometry or reactions.
    for(auto& record:fRecords) {
        auto job=record->job;
        const bool pinned=fSavePanel && fSavePanel->IsShowing() && fSavingToken==job->token;
        if(!job->running && !job->playing && !record->video && job.use_count() == 2 && job->cacheLease && !record->wantsSave && !record->wantsPlay && !pinned
            && !fTimeline.ParagraphVisible(record->paragraph)
            && (fTimeline.IsHidden() || sCachedFiles.load()>=8)) {
            record->displayed.reset();job->ClearCache();job->ready=false;
            record->automatic=Upload::IsPreviewFilename(job->url);
            record->status=record->automatic?"Loading image…":"Attachment. Download or save to view.";
            Render(*record);
        }
    }
    for(auto it=fRecords.rbegin();it!=fRecords.rend();++it) {
        auto& record=**it;
        if(record.automatic && !record.job->running && !record.job->ready
            && fTimeline.ParagraphVisible(record.paragraph)
            && sWorkers.load()<2 && sCachedFiles.load()<8)Start(record);
    }

	for (auto& record : fRecords) {
		auto job = record->job;
		std::string status=record->status;
		if (job->running) {
			status = job->saving ? "Saving…" : "Downloading " + std::to_string(job->bytes.load() / 1024) + " KiB…";
		} else if (!job->error.empty()) {
			status = job->error;
			record->wantsSave = false; record->wantsPlay = false;
		} else if (job->ready) {
			status = job->saved ? "Saved. The downloaded file was not opened automatically."
				: (!job->preview.frames.empty() ? "" : Upload::IsPreviewFilename(job->url) ? "Image preview unavailable. Click for options." : "File downloaded. Click to save or open.");
			if (record->wantsSave) { record->wantsSave = false; SavePanel(*record); }
            if (record->wantsPlay) { record->wantsPlay = false; PlayVideo(*record); }
		}
        if (record->video) {
            if (fTimeline.IsHidden() || record->paragraph < 0) record->video->Pause(true);
            const auto old = record->videoState;
            const auto generation = record->videoGeneration;
            const bool hadPreview = bool(record->displayed);
            record->videoState = record->video->Read(record->displayed, record->videoGeneration);
            const auto& current = record->videoState;
            if (current.stopped && current.error.empty()) {
                record->video.reset(); record->displayed.reset(); Render(*record);
            } else {
                if (hadPreview != bool(record->displayed) || old.paused != current.paused
                    || old.ended != current.ended || old.muted != current.muted
                    || old.ready != current.ready || old.error != current.error
                    || old.position / 1000000 != current.position / 1000000) Render(*record);
                else if (generation != record->videoGeneration) fTimeline.Invalidate();
                continue;
            }
        }
        bool redraw = status != record->status;
        if (!job->running && job->ready && !job->preview.frames.empty() && !record->displayed) {
            record->displayed = std::make_shared<BBitmap>(job->preview.frames[0].get());
            if (record->displayed->InitCheck()!=B_OK) record->displayed.reset();
            else record->previewSize=BSize(record->displayed->Bounds().Width()+1,record->displayed->Bounds().Height()+1);
            record->nextFrame = system_time() + job->preview.delays[0]*1000;
            redraw = true;
        }
        if (redraw) {
            record->status=std::move(status); Render(*record);
            if (!record->status.empty() && !job->running) {
                BMessage notice('aSta');notice.AddString("text",record->status.c_str());
                fTarget.SendMessage(&notice,static_cast<BHandler*>(nullptr),0);
            }
        }
        if (!job->running && record->displayed && job->preview.frames.size()>1
            && !record->paused && fTimeline.ParagraphVisible(record->paragraph) && system_time()>=record->nextFrame) {
            record->frame=(record->frame+1)%job->preview.frames.size();
            const auto& frame=job->preview.frames[record->frame];
            memcpy(record->displayed->Bits(),frame->Bits(),frame->BitsLength());
            record->nextFrame=system_time()+job->preview.delays[record->frame]*1000;
            // Same dimensions, no document/layout churn for animation frames.
            fTimeline.Invalidate();
        }
	}
}

bool AttachmentController::HandleMessage(const BMessage& message)
{
	if (message.what == kPoll) { Poll(); return true; }
	if (message.what != kGet && message.what != kSave && message.what != kCancel && message.what != kSaveRef && message.what != kMenu && message.what != kPlay && message.what != kVideo && message.what != kExternal && message.what != kSeek && message.what != kMute && message.what != kStop) return false;
	uint64 token = message.GetUInt64("attachment-token", 0);
	for (auto& record : fRecords) {
		auto job = record->job;
		if (job->token != token) continue;
        if (message.what == kMenu) {
            auto* menu = new BPopUpMenu("Attachment options",false,false);
            auto add=[&](const char* label,uint32 code) {
                auto* action=new BMessage(code); action->AddUInt64("attachment-token",token);
                if(code=='hope') action->AddString("url",job->url.c_str());
                auto* item=new BMenuItem(label,action);item->SetTarget(fTarget);menu->AddItem(item);
            };
            if(job->running) add("Cancel",kCancel);
            else {
                if (Upload::IsVideoFilename(job->url)) {
                    add("Play / pause video", kVideo);
                    if (job->ready) add("Open in MediaPlayer", kExternal);
                }
                if(!job->ready) add("Download / retry",kGet);
                add(Upload::IsPreviewFilename(job->url)?"Save image…":"Save file…",kSave);
                if(job->preview.frames.size()>1)add(record->paused?"Play animation":"Pause animation",kPlay);
            }
            add(Upload::IsPreviewFilename(job->url)?"Open original image":"Open in browser",'hope');
            BPoint point;uint32 buttons;fTimeline.GetMouse(&point,&buttons);
            fTimeline.ConvertToScreen(&point);
            menu->SetAsyncAutoDestruct(true);menu->Go(point,true,true,true);
            return true;
        } else if (message.what == kExternal) {
            record->video.reset(); record->displayed.reset(); OpenExternalVideo(*record);
        } else if (message.what == kSeek && record->video) {
            record->video->Seek(record->videoState.position + message.GetInt64("seek", 0));
        } else if (message.what == kMute && record->video) {
            record->video->Mute(!record->videoState.muted);
        } else if (message.what == kStop) {
            record->video.reset(); record->displayed.reset(); record->status.clear();
        } else if (message.what == kVideo) {
            if (job->running || !Upload::IsVideoFilename(job->url)) return true;
            if (job->ready) PlayVideo(*record);
            else { record->wantsPlay = true; Start(*record); }
        } else if (message.what == kPlay) {
            record->paused=!record->paused;
        } else if (message.what == kCancel) {
			job->cancelled = true; record->wantsSave = false; record->wantsPlay = false;
		} else if (message.what == kGet) {
			Start(*record);
		} else if (message.what == kSave) {
			if (job->running) return true;
			if (job->ready) SavePanel(*record);
			else { record->wantsSave = true; Start(*record); }
		} else if (message.what == kSaveRef && token == fSavingToken && !job->running && job->ready) {
			entry_ref directory;
			const char* name = message.GetString("name", "");
			if (message.FindRef("directory", &directory) != B_OK || !*name || strchr(name, '/')
				|| !strcmp(name, ".") || !strcmp(name, "..")) return true;
			BPath path(&directory);
			if (path.InitCheck() != B_OK) return true;
			if (!Reserve(sWorkers, 2)) { record->status = "Two transfers are active. Try Save again shortly."; Render(*record); return true; }
			fSavingToken = 0; // Consume the dialog result once.
			record->status = "Saving…"; // A fast repeat-save must still trigger final redraw.
			job->cancelled = false; job->saved = false; job->running = true; job->saving = true;
			// The dialog message and BPath die when this handler returns. Copy
			// both strings before launching the worker, not on the worker thread.
			try { std::thread(Save, job, std::string(path.Path()), std::string(name)).detach(); }
			catch (const std::system_error&) { --sWorkers; job->error = "Could not start save worker."; job->running = false; }
		}
		Render(*record);
		return true;
	}
	return true; // An expired token is harmless.
}
