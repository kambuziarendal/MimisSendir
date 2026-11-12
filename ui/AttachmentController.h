// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include <FilePanel.h>
#include <MessageRunner.h>
#include <Messenger.h>
#include <memory>
#include <string>
#include <vector>
#include "ui/textview/TextDocumentView.h"

// Owned and invoked exclusively by a TalkView looper. Workers own only Jobs.
class AttachmentController {
public:
	AttachmentController(BMessenger target, TextDocumentView& timeline,
		const TextDocumentRef& document, std::string authority, std::string caFile);
	~AttachmentController();
	void Add(const std::string& url);
    void Rebind(const TextDocumentRef& document);
	bool HandleMessage(const BMessage& message);
private:
	struct Job;
	struct Record;
	void Render(Record& record);
	void Start(Record& record);
	void SavePanel(Record& record);
    void PlayVideo(Record& record);
    void OpenExternalVideo(Record& record);
	void Poll();
	static void Download(std::shared_ptr<Job> job, std::string authority, std::string caFile);
	static void Save(std::shared_ptr<Job> job, std::string directory, std::string name);
	BMessenger fTarget;
	TextDocumentView& fTimeline;
	TextDocumentRef fDocument;
	std::string fAuthority;
	std::string fCAFile;
	std::vector<std::unique_ptr<Record>> fRecords;
	BMessageRunner* fTimer;
	BFilePanel* fSavePanel = nullptr;
	uint64 fSavingToken = 0;
};
