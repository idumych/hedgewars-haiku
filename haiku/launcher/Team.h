// The Team tab: frontend-qt6's PageEditTeam. Each team is a Teams/<name>.hwt
// file in the Qt frontend's format, which the engine also reads for the
// team's own key bindings.
#pragma once

#include "Binds.h"
#include "Game.h"

#include <GroupView.h>
#include <Messenger.h>

class BButton;
class BMenuField;
class BTextControl;

class TeamView : public BGroupView {
public:
	TeamView();
	void Load(const BMessage& settings);	// after find_data(); picks the team used last
	void Save(BMessage& settings);	// writes the team's file too
	TeamInfo Team();	// the fields, saved to the team's file
	std::vector<std::string> OtherTeams() const;	// the saved teams but this one
	static TeamInfo SavedTeam(const std::string& name);	// from its file
	void AttachedToWindow() override;
	void MessageReceived(BMessage* msg) override;

private:
	void FillTeams();
	void ShowTeam(const std::string& name);
	void ToFields();
	void FromFields();
	bool Write();

	std::vector<std::string> fHats, fNames, fTeams;
	TeamInfo fTeam;
	Binds fBinds;
	std::string fFileName;	// the team's name when it was loaded: its file
	BMessenger fControls;
	BMenuField *fTeamMenu, *fPlayer, *fFlag, *fGrave, *fFort, *fVoice;
	BButton *fTrash, *fNew, *fControlsButton, *fRandomNames, *fRandomHats;
	BTextControl* fTeamName;
	BTextControl* fHogName[8];
	BMenuField* fHat[8];
};
