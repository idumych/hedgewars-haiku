// Downloadable content, after frontend-qt6/ui/page/pagedata.cpp: the add-ons
// listed on hedgewars.org, downloaded into the user's Data folder, where the
// engine finds them (it mounts the .hwp packages there).
#include "Content.h"

#include <Alert.h>
#include <Button.h>
#include <ColumnListView.h>	// private/interface
#include <ColumnTypes.h>
#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <LayoutBuilder.h>
#include <Roster.h>
#include <StatusBar.h>
#include <StringView.h>
#include <Window.h>

#include <curl/curl.h>

#include "ContentData.h"
#include "Game.h"

static const char* kContentUrl = "https://hedgewars.org/content.html";

enum { kMsgListed = 'clst', kMsgDownload = 'cdwn', kMsgProgress = 'cprg', kMsgDone = 'cdon',
	kMsgOpenFolder = 'cfld', kMsgSel = 'csel' };

static BPath data_dir() { return BPath(gUserDir.Path(), "Data"); }

struct Fetch {
	std::string url, path;	// path: the file to write, or "" to keep it in body
	std::string body, error;
	BMessenger target;
	bigtime_t lastProgress = 0;
};

static size_t write_body(char* data, size_t size, size_t n, void* user)
{
	((Fetch*)user)->body.append(data, size * n);
	return size * n;
}

static int progress(void* user, curl_off_t total, curl_off_t now, curl_off_t, curl_off_t)
{
	Fetch* f = (Fetch*)user;
	if (system_time() - f->lastProgress > 100000 && total > 0) {
		f->lastProgress = system_time();
		BMessage m(kMsgProgress);
		m.AddFloat("fraction", (float)now / total);
		f->target.SendMessage(&m);
	}
	return 0;
}

// fetches f->url; a download goes to a file beside its final name, renamed when complete
static int32 fetch_thread(void* data)
{
	Fetch* f = (Fetch*)data;
	CURL* curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_URL, f->url.c_str());
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "Hedgewars");
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_body);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress);
	curl_easy_setopt(curl, CURLOPT_XFERINFODATA, f);
	CURLcode res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	if (res != CURLE_OK)
		f->error = std::string("Could not download from hedgewars.org (") + curl_easy_strerror(res)
			+ "). Check that you're online, and try again.";
	else if (!f->path.empty()) {
		// shortcut: the package is held in memory; add-ons are at most a few MiB
		std::string tmp = f->path + ".part";
		BFile out(tmp.c_str(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		if (out.InitCheck() != B_OK || out.Write(f->body.data(), f->body.size()) != (ssize_t)f->body.size()
			|| BEntry(tmp.c_str()).Rename(BPath(f->path.c_str()).Leaf(), true) != B_OK)
			f->error = "Could not save the add-on in " + std::string(data_dir().Path()) + ". Check that the disk isn't full.";
	}
	BMessage done(f->path.empty() ? kMsgListed : kMsgDone);
	done.AddPointer("fetch", f);
	if (f->target.SendMessage(&done) != B_OK)
		delete f;	// the window is gone
	return 0;
}

static void start_fetch(BWindow* w, const std::string& url, const std::string& path)
{
	Fetch* f = new Fetch;
	f->url = url;
	f->path = path;
	f->target = BMessenger(w);
	resume_thread(spawn_thread(fetch_thread, "fetch", B_NORMAL_PRIORITY, f));
}

// remembers its package, since the list can be sorted
struct PackageRow : BRow {
	size_t index;
	explicit PackageRow(size_t i) : index(i) {}
};

class ContentWindow : public BWindow {
public:
	ContentWindow()
		: BWindow(BRect(140, 140, 900, 560), "Downloadable content", B_TITLED_WINDOW,
			B_AUTO_UPDATE_SIZE_LIMITS)
	{
		fList = new BColumnListView("add-ons", B_NAVIGABLE, B_FANCY_BORDER);
		const char* cols[] = {"Name", "Kind", "Description", "Size", "Credits", "License", "Installed"};
		float widths[] = {170, 90, 260, 65, 80, 90, 60};
		for (int i = 0; i < 7; i++)
			fList->AddColumn(new BStringColumn(cols[i], widths[i] * be_plain_font->Size() / 12, 30, 600, B_TRUNCATE_END), i);
		fList->SetSelectionMessage(new BMessage(kMsgSel));
		fList->SetInvocationMessage(new BMessage(kMsgDownload));
		fList->SetExplicitMinSize(BSize(500, 250));
		fStatus = new BStatusBar("status");
		fStatus->SetText("Loading the list from hedgewars.org…");
		fDownload = new BButton("Download", new BMessage(kMsgDownload));
		fDownload->SetEnabled(false);
		BStringView* hint = new BStringView("hint", "Add-ons take effect in the next game. Maps, "
			"forts, themes and styles must be installed by everyone in a network game.");
		hint->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
		BLayoutBuilder::Group<>(this, B_VERTICAL)
			.SetInsets(B_USE_WINDOW_SPACING)
			.Add(fList)
			.Add(hint)
			.Add(fStatus)
			.AddGroup(B_HORIZONTAL)
				.Add(new BButton("Open add-ons folder", new BMessage(kMsgOpenFolder)))
				.AddGlue()
				.Add(fDownload)
			.End();
		SetDefaultButton(fDownload);
		CenterOnScreen();
		create_directory(data_dir().Path(), 0755);
		static bool curlReady = curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
		(void)curlReady;
		start_fetch(this, kContentUrl, "");
	}

	void Fill()
	{
		fList->Clear();
		for (size_t i = 0; i < fPackages.size(); i++) {
			const Package& p = fPackages[i];
			bool have = BEntry(BPath(data_dir().Path(), package_file(p).c_str()).Path()).Exists();
			const std::string fields[7] = {p.name, p.kind, p.description, p.size, p.credits, p.license,
				have ? "Yes" : ""};
			BRow* row = new PackageRow(i);
			for (int f = 0; f < 7; f++)
				row->SetField(new BStringField(fields[f].c_str()), f);
			fList->AddRow(row);
		}
	}

	const Package* Selected()
	{
		PackageRow* row = (PackageRow*)fList->CurrentSelection();
		return row && row->index < fPackages.size() ? &fPackages[row->index] : NULL;
	}

	void MessageReceived(BMessage* msg) override
	{
		switch (msg->what) {
			case kMsgListed:
			case kMsgDone: {
				Fetch* f;
				if (msg->FindPointer("fetch", (void**)&f) != B_OK)
					break;
				fBusy = false;
				fStatus->Reset();
				if (!f->error.empty())
					fStatus->SetText(f->error.c_str());
				else if (msg->what == kMsgListed) {
					fPackages = parse_content(f->body);
					fStatus->SetText(fPackages.empty() ? "hedgewars.org lists no add-ons right now."
						: (std::to_string(fPackages.size()) + " add-ons").c_str());
					Fill();
				} else {
					fStatus->SetText(("Installed " + std::string(BPath(f->path.c_str()).Leaf())).c_str());
					// mark it in place, keeping the list's scrolling and selection
					for (int32 i = 0; BRow* row = fList->RowAt(i); i++)
						if (package_file(fPackages[((PackageRow*)row)->index]) == BPath(f->path.c_str()).Leaf()) {
							row->SetField(new BStringField("Yes"), 6);
							fList->UpdateRow(row);
						}
				}
				fDownload->SetEnabled(Selected() != NULL);
				delete f;
				break;
			}
			case kMsgSel: fDownload->SetEnabled(!fBusy && Selected() != NULL); break;
			case kMsgDownload: {
				const Package* p = Selected();
				if (!p || fBusy)
					break;
				fBusy = true;
				fDownload->SetEnabled(false);
				fStatus->Reset();
				fStatus->SetText(("Downloading " + p->name + "…").c_str());
				start_fetch(this, package_url(*p), BPath(data_dir().Path(), package_file(*p).c_str()).Path());
				break;
			}
			case kMsgProgress: {
				float fraction = msg->GetFloat("fraction", 0);
				fStatus->Update(fraction * 100 - fStatus->CurrentValue());
				break;
			}
			case kMsgOpenFolder: {
				entry_ref ref;
				if (get_ref_for_path(data_dir().Path(), &ref) == B_OK) {
					BMessage open(B_REFS_RECEIVED);
					open.AddRef("refs", &ref);
					be_roster->Launch("application/x-vnd.Be-TRAK", &open);
				}
				break;
			}
			default:
				BWindow::MessageReceived(msg);
		}
	}

private:
	std::vector<Package> fPackages;
	BColumnListView* fList;
	BStatusBar* fStatus;
	BButton* fDownload;
	bool fBusy = false;
};

void open_content_window()
{
	static BMessenger sWindow;
	BLooper* looper = NULL;
	sWindow.Target(&looper);
	if (BWindow* w = dynamic_cast<BWindow*>(looper)) {
		if (w->Lock()) {
			w->Activate();
			w->Unlock();
		}
		return;
	}
	ContentWindow* w = new ContentWindow;
	sWindow = BMessenger(w);
	w->Show();
}
