/* ScummVM - Scumm Interpreter
 * Copyright (C) 2003 The ScummVM project
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * $Header$
 *
 */

#include "stdafx.h"
#include "queen/talk.h"
#include "queen/display.h"
#include "queen/graphics.h"
#include "queen/logic.h"
#include "queen/sound.h"

namespace Queen {

/*
	 Functions needed:

	 Data needed:

 */

void Talk::talk(
		const char *filename, 
		int personInRoom,
		char *cutawayFilename,
		Graphics *graphics,
		Input *input,
		Logic *logic,
		Resource *resource,
		Sound *sound) {
	Talk *talk = new Talk(graphics, input, logic, resource, sound);
	talk->talk(filename, personInRoom, cutawayFilename);
	delete talk;
}

bool Talk::speak(
		const char *sentence, 
		Person *person,
		const char *voiceFilePrefix,
		Graphics *graphics,
		Input *input,
		Logic *logic,
		Resource *resource,
		Sound *sound) {
	Talk *talk = new Talk(graphics, input, logic, resource, sound);
	bool result;
	if (sentence)
		result = talk->speak(sentence, person, voiceFilePrefix);
	else
		result = false;
	delete talk;
	return result;
}

Talk::Talk(
		Graphics *graphics,
		Input *input,
		Logic *logic,
		Resource *resource,
		Sound *sound) : 
	_graphics(graphics), _input(input), _logic(logic), _resource(resource), 
	_sound(sound), _fileData(NULL) {

	_input->talkQuitReset();
}

Talk::~Talk() {
	delete[] _fileData;
}

void Talk::talk(const char *filename, int personInRoom, char *cutawayFilename) {
	int i;
	_oldSelectedSentenceIndex = 0;
	_oldSelectedSentenceValue = 0;

	debug(0, "----- talk(\"%s\") -----", filename);

	cutawayFilename[0] = '\0';

	// XXX S=SUBJECT[1];

	int roomStart = _logic->roomData(_logic->currentRoom());
	ObjectData *data = _logic->objectData(roomStart + personInRoom);

	if (data->name <= 0)	// disabled!
		return;

	if (data->entryObj > 0)
		return;
	
	// XXX R=ROOM_DATA[ROOM];
	// XXX if(OBJECT_DATA[NOUN2+R][0]<=0) return;
	// XXX if(OBJECT_DATA[NOUN2+R][4]>0) return;
	// XXX strcpy(Pstr,FIND_STATE(OBJECT_DATA[NOUN2+R][6],"TALK"));

	// I cant talk to that.

	// XXX if(seq(Pstr,"MUTE")) {
	// XXX 	k=24+Rnd(2);
	// XXX 	SPEAK(JOE_RESPstr[k],"JOE",find_cd_desc(k));
	// XXX 	return;
	// XXX }

	load(filename);

	Person person;
	memset(&person, 0, sizeof(Person));
	_logic->personSetData(personInRoom, "", false, &person);

	if (NULL == person.name) {
		error("Invalid person object");
	}

	int16 oldLevel = 0;
	bool personWalking = false;		// OWALK in talk.c

	// Lines 828-846 in talk.c
	for (i = 1; i <= 4; i++) {
		if (talkSelected()->values[i-1] > 0) {
			// This option has been redefined so display new dialogue option
			_dialogueTree[1][i].head = talkSelected()->values[i-1];
		}
		else if (talkSelected()->values[i-1] == -1) {

			// Already selected so don't redisplay
			if (_dialogueTree[1][i].gameStateIndex >= 0) {
				_dialogueTree[1][i].head = -1;
				_dialogueTree[1][i].dialogueNodeValue1 = -1;
				_dialogueTree[1][i].gameStateIndex = -1;
				_dialogueTree[1][i].gameStateValue = -1;
			}
		}
	}

	initialTalk();

	// Lines 906-? in talk.c
	_logic->display()->mouseCursorShow(true);
	
	int16 level=1, retval=0;
	int16 head = _dialogueTree[level][0].head;

	// TODO: split this loop in several functions
	while(retval != -1) {
		// debug(0, "retval = %i", retval);
		
		char otherVoiceFilePrefix    [MAX_STRING_SIZE];

		_talkString[0][0] = '\0';

		if(talkSelected()->hasTalkedTo == 1 && head == 1)
			strcpy(_talkString[0], _person2String);
		else
			findDialogueString(_person1Ptr, head, _talkString[0]);

		if(talkSelected()->hasTalkedTo == 1 && head == 1)
			sprintf(otherVoiceFilePrefix, "%2dXXXXP", _talkKey);
		else
			sprintf(otherVoiceFilePrefix, "%2d%4xP", _talkKey, head);

		if (_talkString[0][0] == '\0' && retval > 1) {
			findDialogueString(_person1Ptr, retval, _talkString[0]);
			sprintf(otherVoiceFilePrefix,"%2d%4xP", _talkKey, retval);
		}

		// Joe dialogue

		for (i = 1; i <= 4; i++) {
			findDialogueString(_joePtr, _dialogueTree[level][i].head, _talkString[i]);

			int16 index = _dialogueTree[level][i].gameStateIndex;

			if (index < 0 && _logic->gameState(abs(index)) != _dialogueTree[level][i].gameStateValue)
				_talkString[i][0] = '\0';

			sprintf(_joeVoiceFilePrefix[i], "%2d%4xJ", _talkKey, _dialogueTree[level][i].head);
		}

		// Check to see if(all the dialogue options have been selected.
		// if this is the case, and the last one left is the exit option,
		// then automatically set S to that and exit.

		int choicesLeft = 0;
		int selectedSentence = 0;

		for (i = 1; i <= 4; i++) {
			if (_talkString[i][0] != '\0') {
				choicesLeft++;
				selectedSentence = i;
			}
		}

		// debug(0, "choicesLeft = %i", choicesLeft);

		if (1 == choicesLeft) {
			// Automatically run the final dialogue option
			if (speak(_talkString[0], &person, otherVoiceFilePrefix))
				personWalking = true;

			if (_input->talkQuit())
				break;

			speak(_talkString[selectedSentence], NULL, _joeVoiceFilePrefix[selectedSentence]);
		}
		else {
			if (person.actor->bobNum > 0) {
				speak(_talkString[0], &person, otherVoiceFilePrefix);
				selectedSentence = selectSentence();
			}
			else {
				warning("bobBum is %i", person.actor->bobNum);
				selectedSentence = 0;
			}
		}

		if (_input->talkQuit())
			break;

		retval   = _dialogueTree[level][selectedSentence].dialogueNodeValue1;
		head     = _dialogueTree[level][selectedSentence].head;
		oldLevel = level;
		level    = 0;

		// Set LEVEL to the selected child in dialogue tree
		
		for (i = 1; i <= _levelMax; i++)
			if (_dialogueTree[i][0].head == head)
				level = i;

		if (0 == level) {
			// No new level has been selected, so lets set LEVEL to the
			// tree path pointed to by the RETVAL

			for (i = 1; i <= _levelMax; i++)
				for (int j = 0; j <= 5; j++)
					if (_dialogueTree[i][j].head == retval)
						level = i;

			disableSentence(oldLevel, selectedSentence);
		}
		else { // 0 != level
			// Check to see if Person Return value is positive, if it is, then
			// change the selected dialogue option to the Return value

			if (_dialogueTree[level][0].dialogueNodeValue1 > 0) {
				if (1 == oldLevel) {
						_oldSelectedSentenceIndex = selectedSentence;
						_oldSelectedSentenceValue = talkSelected()->values[selectedSentence-1];
						talkSelected()->values[selectedSentence-1] = _dialogueTree[level][0].dialogueNodeValue1;
				}

				_dialogueTree[oldLevel][selectedSentence].head = _dialogueTree[level][0].dialogueNodeValue1;
				_dialogueTree[level][0].dialogueNodeValue1 = -1;
			}
			else {
				disableSentence(oldLevel, selectedSentence);
			}
		}

		// Check selected person to see if any Gamestates need setting

		int16 index = _dialogueTree[level][0].gameStateIndex;

		if (index > 0)
			_logic->gameState(index, _dialogueTree[level][0].gameStateValue);

		// if the selected dialogue line has a POSITIVE game state value
		// then set gamestate to Value = TALK(OLDLEVEL,S,3)
		
		index = _dialogueTree[oldLevel][selectedSentence].gameStateIndex;
		if (index > 0)
			_logic->gameState(index, _dialogueTree[oldLevel][selectedSentence].gameStateValue);


		// if(RETVAL = -1, then before we exit, check to see if(person
		// has something final to say!

		if (-1 == retval) {
			findDialogueString(_person1Ptr, head, _talkString[0]);
			if (_talkString[0][0] != '\0') {
				sprintf(otherVoiceFilePrefix, "%2d%4xP", _talkKey, head);
				if (speak(_talkString[0], &person, otherVoiceFilePrefix))
					personWalking = true;
			}
		}
	}

// TALK_PROC_EXIT:

	cutawayFilename[0] = '\0';

	for (i = 0; i < 2; i++) {
		if (_gameState[i] > 0) {
			if (_logic->gameState(_gameState[i]) == _testValue[i]) {
				if (_itemNumber[i] > 0)
					_logic->inventoryInsertItem(_itemNumber[i]);
				else
					_logic->inventoryDeleteItem(abs(_itemNumber[i]));
			}
		}
	}
	
	_logic->zoneSetupPanel();

	uint8 *ptr = _cutawayPtr;

	int16 cutawayGameState = (int16)READ_BE_UINT16(ptr); ptr += 2;
	int16 cutawayTestValue = (int16)READ_BE_UINT16(ptr); ptr += 2;

	if (_logic->gameState(cutawayGameState) == cutawayTestValue) {
		getString(ptr, cutawayFilename, 20);

		//CR 2 - 7/3/95, If we're executing a cutaway scene, then make sure
		// Joe can talk, so set TALKQUIT to 0 just in case we exit on the
		// line that set's the cutaway game states.
		_input->talkQuitReset();
	}

	if (_input->talkQuit()) {
		talkSelected()->hasTalkedTo = 1;
	}
	else {
		// TODO: missing some code here!
	}

	_logic->joeFace();

	// TODO: missing some code here!

	if (cutawayFilename[0] == '\0') {
		BobSlot *pbs = _graphics->bob(person.actor->bobNum);

		pbs->x = person.actor->x;
		pbs->y = person.actor->y;
		
		// Better kick start the persons anim sequence
		pbs->animating = true;
		pbs->animReset();
	}

	_talkHead = false;
	_logic->joeWalk(JWM_NORMAL);
}
		
void Talk::disableSentence(int oldLevel, int selectedSentence) {
	// Mark off selected option

	if (1 == oldLevel) {
		if (_dialogueTree[oldLevel][selectedSentence].dialogueNodeValue1 != -1) {
			// Make sure choice is not exit option
			_oldSelectedSentenceIndex = selectedSentence;
			_oldSelectedSentenceValue = talkSelected()->values[selectedSentence-1];
			talkSelected()->values[selectedSentence-1] = -1;
		}
	}

	// Cancel selected dialogue line, so that its no longer displayed

	_dialogueTree[oldLevel][selectedSentence].head = -1;
	_dialogueTree[oldLevel][selectedSentence].dialogueNodeValue1 = -1;
}

void Talk::findDialogueString(byte *ptr, int16 id, char *str) {
	str[0] = '\0';

	for (int i = 1; i <= _pMax; i++) {
		ptr += 2;
		int16 currentId = (int16)READ_BE_UINT16(ptr); ptr += 2;
		if (id == currentId) {
			ptr = getString(ptr, str, MAX_STRING_LENGTH, 4);
			//debug(0, "Found string with ID %i: '%s'", id, str);
			break;
		}
		else
			ptr = getString(ptr, NULL, MAX_STRING_LENGTH, 4);
	}

	if (str[0] == '\0')
		warning("Failed to find string with ID %i", id);
}

void Talk::load(const char *filename) {
	int i;
	
	byte *ptr = _fileData = _resource->loadFile(filename, 20);
	if (!_fileData) {
		error("Failed to load resource data file '%s'", filename);
	}

	bool canQuit;

	//
	// Load talk header
	//

	_levelMax = (int16)READ_BE_UINT16(ptr); ptr += 2;

	//debug(0, "levelMax = %i", _levelMax);

	if (_levelMax < 0) {
		_levelMax = -_levelMax;
		canQuit = false;
	}
	else
		canQuit = true;

	_uniqueKey      = (int16)READ_BE_UINT16(ptr); ptr += 2;
	_talkKey        = (int16)READ_BE_UINT16(ptr); ptr += 2;
	_jMax           = (int16)READ_BE_UINT16(ptr); ptr += 2;
	_pMax           = (int16)READ_BE_UINT16(ptr); ptr += 2;

	for (i = 0; i < 2; i++) {
		_gameState [i] = (int16)READ_BE_UINT16(ptr); ptr += 2;
		_testValue [i] = (int16)READ_BE_UINT16(ptr); ptr += 2;
		_itemNumber[i] = (int16)READ_BE_UINT16(ptr); ptr += 2;
	}

	//debug(0, "uniqueKey = %i", _uniqueKey);
	//debug(0, "talkKey   = %i", _talkKey);

	_person1Ptr = _fileData + READ_BE_UINT16(ptr); ptr += 2;
	_cutawayPtr = _fileData + READ_BE_UINT16(ptr); ptr += 2;
	_person2Ptr = _fileData + READ_BE_UINT16(ptr); ptr += 2;

	if (ptr != (_fileData + 28))
		error("ptr != (_fileData + 28))");
	
	byte *dataPtr    = _fileData + 32;
	_joePtr          = dataPtr + _levelMax * 96;
	
	//
	// Load dialogue tree
	//

	ptr = dataPtr;

	for (i = 1; i <= _levelMax; i++)
		for (int j = 0; j <= 5; j++) {
			ptr += 2;
			_dialogueTree[i][j].head = (int16)READ_BE_UINT16(ptr); ptr += 2;
			ptr += 2;
			_dialogueTree[i][j].dialogueNodeValue1 = (int16)READ_BE_UINT16(ptr); ptr += 2;
			ptr += 2;
			_dialogueTree[i][j].gameStateIndex = (int16)READ_BE_UINT16(ptr); ptr += 2;
			ptr += 2;
			_dialogueTree[i][j].gameStateValue = (int16)READ_BE_UINT16(ptr); ptr += 2;
		}
}

void Talk::initialTalk() {
	// Lines 848-903 in talk.c 

	byte *ptr = _joePtr + 2;
	
	uint16 hasNotString = READ_BE_UINT16(ptr); ptr += 2;

	char joeString[MAX_STRING_SIZE];
	if (!hasNotString) {
		ptr = getString(ptr, joeString, MAX_STRING_LENGTH);
		//debug(0, "joeString = '%s'", joeString);
	}
	else
		joeString[0] = '\0';

	ptr = _person2Ptr;
	ptr = getString(ptr, _person2String, MAX_STRING_LENGTH);
	//debug(0, "person2String = '%s'", _person2String);

	char joe2String[MAX_STRING_SIZE];
	ptr = getString(ptr, joe2String, MAX_STRING_LENGTH);
	//debug(0, "joe2String = '%s'", joe2String);

	if (talkSelected()->hasTalkedTo == 0) {
		
		// Not yet talked to this person
		
		if (joeString[0] != '0') {
			char voiceFilePrefix[MAX_STRING_SIZE];
			sprintf(voiceFilePrefix, "%2dSSSSJ", _talkKey);
			speak(joeString, NULL, voiceFilePrefix);
		}
	}
	else {
		// Already spoken to them, choose second response
		
		if (joe2String[0] != '0') {
			char voiceFilePrefix[MAX_STRING_SIZE];
			sprintf(voiceFilePrefix, "%2dSSSSJ", _talkKey);
			speak(joe2String, NULL, voiceFilePrefix);
		}

	}

}

int Talk::getSpeakCommand(const char *sentence, unsigned &index) {
	// Lines 1299-1362 in talk.c
	int commandCode = SPEAK_DEFAULT;

	switch (sentence[index]) {
		case 'A':
			if (sentence[index + 1] == 'O')
				commandCode = SPEAK_AMAL_ON;
			else
				warning("Unknown command string: '%2s'", sentence + index);
			break;
			
		case 'F':
			switch (sentence[index + 1]) {
				case 'L':
					commandCode = SPEAK_FACE_LEFT;
					break;
				case 'F':
					commandCode = SPEAK_FACE_FRONT;
					break;
				case 'B':
					commandCode = SPEAK_FACE_BACK;
					break;
				case 'R':
					commandCode = SPEAK_FACE_RIGHT;
					break;
				default:
					warning("Unknown command string: '%2s'", sentence + index);
					break;
			}
			break;

		case 'G':
			switch (sentence[index + 1]) {
				case 'D':
					_logic->joeGrabDirection(STATE_GRAB_DOWN, 0);
					break;
				case 'M':
					_logic->joeGrabDirection(STATE_GRAB_MID, 0);
					break; 
				default:
					warning("Unknown command string: '%2s'", sentence + index);
					break;
			}
			commandCode = SPEAK_NONE;
			break;

		case 'W':
			if (sentence[index + 1] == 'T')
				commandCode = SPEAK_PAUSE;
			else
				warning("Unknown command string: '%2s'", sentence + index);
			break;
			
		case 'X':
			// For example *XY00(237,112)
			if (sentence[index + 1] == 'Y') {
				commandCode = atoi(sentence + index + 2);
				int x = atoi(sentence + index + 5);
				int y = atoi(sentence + index + 9);
				debug(0, "Calling MOVE_SPEAK(person, %i, %i)",x, y);
				// XXX MOVE_SPEAK(person, x, y)
				index += 11;
				/// XXX personWalking = true;
			}
			else
				warning("Unknown command string: '%2s'", sentence + index);
			break;

		default:
			if (sentence[index + 0] >= '0' && sentence[index + 0] <= '9' &&
					sentence[index + 1] >= '0' && sentence[index + 1] <= '9') {
				commandCode = (sentence[index] - '0') * 10 + (sentence[index + 1] - '0');
			}
			else
				warning("Unknown command string: '%2s'", sentence + index);
	}

	index += 2;

	return commandCode;
}


bool Talk::speak(const char *sentence, Person *person, const char *voiceFilePrefix) {
	// Function SPEAK, lines 1266-1384 in talk.c
	bool personWalking = false;
	unsigned segmentIndex = 0;
	unsigned segmentStart = 0;
	unsigned i;

	Person joe_person;
	ActorData joe_actor;

	_logic->joeWalk(JWM_SPEAK);

	if (!person) {
		// Fill in values for use by speakSegment() etc.
		memset(&joe_person, 0, sizeof(Person));
		memset(&joe_actor, 0, sizeof(ActorData));

		joe_actor.bobNum = 0;
		joe_actor.color = 14;
		joe_actor.bankNum = 7;
		
		joe_person.actor = &joe_actor;
		joe_person.name = "JOE";

		person = &joe_person;
	}
	
	//debug(0, "Sentence '%s' is said by person '%s' and voice files with prefix '%s' played",
	//		sentence, person->name, voiceFilePrefix);

	if (sentence[0] == '\0') {
		goto exit;
	}

	if (0 == strcmp(person->name, "FAYE-H") ||
			0 == strcmp(person->name, "FRANK-H") ||
			0 == strcmp(person->name, "AZURA-H") ||
			0 == strcmp(person->name, "X3_RITA")) 
		_talkHead = true;
	else
		_talkHead = false;

	// XXX CLEAR_COMMAND(false)

	for (i = 0; i < strlen(sentence); ) {
		if (sentence[i] == '*') {
			int segmentLength = i - segmentStart;

			i++;
			int command = getSpeakCommand(sentence, i);

			if (SPEAK_NONE != command) {
				speakSegment(
						sentence + segmentStart, 
						segmentLength,
						person,
						command,
						voiceFilePrefix,
						segmentIndex);
				// XXX if (JOEWALK == 2) break
			}

			segmentIndex++;
			segmentStart = i;
		}
		else
			i++;

		if (_input->cutawayQuit() || _input->talkQuit())
			goto exit;
	}

	if (segmentStart != i) {
		speakSegment(
				sentence + segmentStart, 
				i - segmentStart,
				person,
				0,
				voiceFilePrefix,
				segmentIndex);
	}

exit:
	return personWalking;
}

int Talk::countSpaces(const char *segment) {
	int tmp = 0;

	while (*segment++)
		tmp++;
	
	if (tmp < 10)
		tmp = 10;

	return (tmp * 2) / _logic->talkSpeed();
}

void Talk::headStringAnimation(const SpeechParameters *parameters, int bobNum, int bankNum) {
	// talk.c lines 1612-1635
	BobSlot *bob2 = _graphics->bob(2);
	
	if (parameters->animation[0] == 'E') {
		int offset = 1;
	
		BobSlot *bob  = _graphics->bob(bobNum);

		int16 x = bob->x;
		int16 y = bob->y;

		for (;;) {
			uint16 frame;

			sscanf(parameters->animation + offset, "%3hu", &frame);
			if (!frame)
				break;

			offset += 4;

			_graphics->bankUnpack(frame, _logic->numFrames(), bankNum);

			bob2->frameNum = _logic->numFrames();
			bob2->scale = 100;
			bob2->active = true;
			bob2->x = x;
			bob2->y = y;

			_logic->update();
		}
	}
	else
		bob2->active = false;
}

void Talk::stringAnimation(const SpeechParameters *parameters, int startFrame, int bankNum) {
	// lines 1639-1690 in talk.c

	int offset = 0;
	bool torso;

	if (parameters->animation[0] == 'T') {
		// Torso animation
		torso = true;
		_graphics->bankOverpack(parameters->body, startFrame, bankNum);
		offset++;
	}
	else if (parameters->animation[0] == 'E') {
		// Talking head animation
		return;
	}
	else if (!isdigit(parameters->animation[0])) {
		debug(0, "Error in speak string animation: '%s'", parameters->animation);
		return;
	}
	else
		torso = false;

	for (;;) {
		uint16 frame;

		sscanf(parameters->animation + offset, "%3hu", &frame);
		if (!frame)
			break;

		offset += 4;

		if (frame > 500) {
			frame -= 500;
			// XXX #ifdef __DOS__
			// XXX 					if(SFXTOGGLE)
			// XXX 						sfxplay(NULLstr);
			// XXX #endif
		}

		if (torso) {
			_graphics->bankOverpack(frame, startFrame, bankNum);
		}
		else {
			_graphics->bankUnpack(frame, startFrame, bankNum);
			// XXX bobs[BNUM].scale=SF;
		}

		_logic->update();
	}

	// XXX #ifdef __DOS__
	// XXX 			if (VOICETOGGLE && (sfxflag==0))
	// XXX 				while (sfxbusy() && KEYVERB!=101)
	// XXX 					update();
	// XXX #endif
}

void Talk::defaultAnimation(
		const char *segment,
		bool isJoe,
		const SpeechParameters *parameters,
		int startFrame, 
		int bankNum) {
	// lines 1730-1823 in talk.c

#if 0
	debug(0, "Talk::defaultAnimation(\"%s\", %s, {\"%s\", %i, ...}, %i, %i)",
			segment, 
			isJoe ? "true" : "false",
			parameters->name,
			parameters->state,
			startFrame,
			bankNum);
#endif

	if (segment[0] != 0)  {

		int bf;

		if (segment[0] == ' ')
			bf = 0;
		else
			bf = parameters->bf;

		// XXX #ifdef __DOS__
		// XXX     // 02-21-95 03:44pm DOn't talk until sfx finished
		// XXX     if(SFXTOGGLE && VOICETOGGLE) {
		// XXX        if(TEXTTOGGLE==0)
		// XXX            blanktexts(0,150);
		// XXX        while(sfxbusy() && KEYVERB!=101)
		// XXX            update();
		// XXX    }

		// XXX    sfxflag=VOICETOGGLE ? sfxplay(SPKstr) : 1;
		// XXX    if((sfxflag==0) && (TEXTTOGGLE==0)) 
		// XXX		blanktexts(0,150);
		// XXX  #endif

		// Why on earth would someone name a variable qzx?
		short qzx = 0;

		int spaces = countSpaces(segment);

		int i;
		for (i = 0; i < (spaces + 1) /* || sfxflag == 0*/; i++) {
			// XXX #ifdef __DOS__
			// XXX         if(sfxflag==0 && sfxbusy())
			// XXX 			break;
			// XXX #endif

			int head;

			if (parameters->rf > 0)
				head = bf + Logic::randomizer.getRandomNumber(parameters->rf);
			else
				head = bf;

			if (!(_talkHead && isJoe)) {
				if (bf > 0) {
					// Make the head move
					qzx ^= 1;
					if (parameters->af && qzx)
						_graphics->bankOverpack(parameters->af + head, startFrame, bankNum);
					else {
						_graphics->bankOverpack(head, startFrame, bankNum);
					}
				}
				else {
					debug(0, "[Talk::defaultAnimation] Body action!");
					// Just do a body action
					_graphics->bankOverpack(parameters->body, startFrame, bankNum);
				}

				if (!_talkHead)
					_logic->update();
			}
			else
				_logic->update();

			if (_logic->joeWalk() == JWM_SPEAK) {
				if (_input->talkQuit())
					break;

				_logic->update();
			}
			else {
				if (_input->talkQuit())
					break;

				_logic->checkPlayer();
				if (_logic->joeWalk() == JWM_EXECUTE)
					// Selected a command, so exit
					break;
			}

			// Skip through text more quickly
			if (_input->keyVerb().isSkipText()) {
				_input->clearKeyVerb();
				break;
			}
		}
	}

	// Make sure that Person closes their mouths
	if (!isJoe && parameters->ff > 0)
		_graphics->bankOverpack(parameters->ff, startFrame, bankNum);
}


void Talk::speakSegment(
		const char *segmentStart, 
		int length, 
		Person *person,
		int command,
		const char *voiceFilePrefix,
		int index)
{
	// Function SPEAK_SUB, lines 1406-1870 in talk.a

	int i;
	char segment[MAX_STRING_SIZE];
	memcpy(segment, segmentStart, length);
	segment[length] = '\0';
	
	char voiceFileName[MAX_STRING_SIZE];
	snprintf(voiceFileName, sizeof(voiceFileName), "%s%1x", voiceFilePrefix, index + 1);

	// debug(0, "Sentence segment '%*s' is said by person '%s' and voice file '%s' is played",
	//		length, segment, person->name, voiceFileName);

	_sound->sfxPlay(voiceFileName);
	//debug(0, "Playing voice file '%s'", voiceFileName);

	int faceDirectionCommand = 0;

	switch (command) {
		case SPEAK_PAUSE:
			for (i = 0; i < 10; i++) {
				if (_input->talkQuit())
					break;
				_logic->update();
			}
			return;

		case SPEAK_FACE_LEFT:
		case SPEAK_FACE_RIGHT:
		case SPEAK_FACE_FRONT:
		case SPEAK_FACE_BACK:
			faceDirectionCommand = command;	
			command = 0;
			break;
	}

	bool isJoe = (0 == person->actor->bobNum);

	int16  bobNum  = person->actor->bobNum;
	uint16 color   = person->actor->color;
	uint16 bankNum = person->actor->bankNum;

	BobSlot *bob = _graphics->bob(bobNum);

	bool oracle = false;
	int textX = 0;
	int textY = 0;

	if (!isJoe) {
		if (SPEAK_AMAL_ON == command) {
			// It's the oracle!
			// Dont turn AMAL animation off, and dont manually anim person
			command = SPEAK_ORACLE;

			warning("Oracle not yet handled!");
			oracle = true;
			// XXX k=PERSON_FRAMES[BNUM];
			// XXX for(i=5;i<=8;i++) {
			// XXX 	unpack(i,k,BANK);
			// XXX 	k=k+1;
			// XXX }
		}
		else {
			bob->animating = false;
			bob->frameNum = 29 + bobNum + FRAMES_JOE_XTRA;
		}
	}

	if (_talkHead) {
		// talk.c lines 1491-1533
		warning("Text position for talking heads not yet handled");
	}
	else {
		textX = bob->x;
		textY = bob->y;
	}

	//int SF = _logic->findScale(textX, textY);

	const SpeechParameters *parameters = NULL;
	int startFrame = 0;

	if (_talkHead && isJoe) {
		_graphics->bobSetText(bob, segment, textX, textY, color, (_talkHead == true));
		// XXX  hey, parameters will be NULL here!
		defaultAnimation(segment, isJoe, parameters, startFrame, bankNum);
	}
	else {
		if (SPEAK_UNKNOWN_6 == command)
			return;

		if (isJoe) {
			if (_logic->currentRoom() == 108)
				parameters = findSpeechParameters("JOE-E", command, 0);
			else
				parameters = findSpeechParameters("JOE", command, _logic->joeFacing());
		}
		else
			parameters = findSpeechParameters(person->name, command, 0);

		startFrame = 29 + bobNum + FRAMES_JOE_XTRA;
		int faceDirection = 0;

		if (isJoe && _logic->joeFacing() == DIR_LEFT)
			faceDirection = DIR_LEFT;
		else if (!isJoe) {
			ObjectData *data = _logic->objectData(_logic->objectForPerson(bobNum));

			if (data->image == -3)
				faceDirection = DIR_LEFT;

			if (faceDirectionCommand == SPEAK_FACE_LEFT)
				data->image = -3;
			else if (faceDirectionCommand == SPEAK_FACE_RIGHT)
				data->image = -4;
		}

		if (faceDirectionCommand) {
			switch (faceDirectionCommand) {
				case SPEAK_FACE_LEFT:  faceDirection = DIR_LEFT;  break;
				case SPEAK_FACE_RIGHT: faceDirection = DIR_RIGHT; break;
				case SPEAK_FACE_FRONT: faceDirection = DIR_FRONT; break;
				case SPEAK_FACE_BACK:  faceDirection = DIR_BACK;  break;
			}
			if (isJoe)
				_logic->joeFacing(faceDirection);
		}

		if (!isJoe) {
			bob->xflip = (faceDirection == DIR_LEFT);
		}

		// Run animated sequence if SANIMstr is primed

		if (_talkHead) {
			// talk.c lines 1612-1635
			headStringAnimation(parameters, bobNum, bankNum);
		}

		_graphics->bobSetText(bob, segment, textX, textY, color, (_talkHead == true));

		if (parameters->animation[0] != '\0' && parameters->animation[0] != 'E') {
			stringAnimation(parameters, startFrame, bankNum);
		}
		else {
			_graphics->bankUnpack(parameters->body, startFrame, bankNum);

			if (length == 0 && !isJoe && parameters->bf > 0) {
				_graphics->bankOverpack(parameters->bf, startFrame, bankNum);
				_logic->update();
			}

			/* A12 = the frame pointer for the full body frame, well use this  */
			/* for Hot Spot reference, before we have to set up a Torso frame. */
			/* This way the hot spot is at bottom of body */

			// XXX A12=A1;

			if (-1 == parameters->rf) {
				// Setup the Torso frames
				_graphics->bankOverpack(parameters->bf, startFrame, bankNum);
				if (isJoe)
					parameters = findSpeechParameters(person->name, 0, _logic->joeFacing());
				else
					parameters = findSpeechParameters(person->name, 0, 0);
			}

			if (-2 == parameters->rf) {
				// Setup the Torso frames
				_graphics->bankOverpack(parameters->bf, startFrame, bankNum);
				if (isJoe)
					parameters = findSpeechParameters(person->name, 14, _logic->joeFacing());
				else
					parameters = findSpeechParameters(person->name, 14, 0);
			}

			defaultAnimation(segment, isJoe, parameters, startFrame, bankNum);
		}
	}

	// Moved here so that Text is cleared when a Torso command done!
	_graphics->textClear(0,198);

	if (oracle) {
		// lines 1831-1339 in talk.c
		warning("Oracle not yet handled!");
	}

	// Ensure that the correct buffer frame is selected

	if (isJoe && !_talkHead) {
		if (_logic->joeFacing() == DIR_FRONT ||
				_logic->joeFacing() == DIR_BACK) {
			// Joe is facing either Front or Back!
			//  - Don't FACE_JOE in room 69, because Joe is probably
			//       holding the Dino Ray gun.
			if (_logic->currentRoom() != 69) 
				_logic->joeFace();
		}
		else {
			if (command == SPEAK_DEFAULT ||
					command == 6 ||
					command == 7) {
				_logic->joeFace();
			}
			else if (command != 5) {
				// 7/11/94, Ensure that correct mouth closed frame is used!
				if (parameters->rf != -1)
					// XXX should really be just "bf", but it is not always calculated... :-(
					_graphics->bankOverpack(parameters->bf, startFrame, bankNum);
				
				if (parameters->ff == 0)
					_graphics->bankOverpack(10, startFrame, bankNum);
				else
					_graphics->bankOverpack(parameters->ff, startFrame, bankNum);
			}
		}
	}

	_logic->update();
}

const Talk::SpeechParameters *Talk::findSpeechParameters(
		const char *name, 
		int state, 
		int faceDirection) {
	// function FIND_SACTION in queen.c
	const SpeechParameters *iterator = _speechParameters;
	if (faceDirection == DIR_RIGHT)
		faceDirection = DIR_LEFT;

	while (iterator->name[0] != '*') {
		if (0 == scumm_stricmp(iterator->name, name) &&
				iterator->state == state &&
				iterator->faceDirection == faceDirection)
			break;
		iterator++;
	}

	return iterator;
}

byte *Talk::getString(byte *ptr, char *str, int maxLength, int align) {
	int length = *ptr;
	ptr++;

	if (length > maxLength) {
		error("String too long. Length = %i, maxLength = %i, str = '%*s'",
				length, maxLength, length, (const char*)ptr);
	}
	else if (length) {
		if (str)
			memcpy(str, (const char*)ptr, length);
		ptr += length;

		while ((int)ptr % align)
			ptr++;
	}

	if (str)
		str[length] = '\0';

	return ptr;
}

TalkSelected *Talk::talkSelected() {
	return _logic->talkSelected(_uniqueKey);
}

int Talk::splitOption(const char *str, char optionText[5][MAX_STRING_SIZE]) {

	//debug(0, "splitOption(\"%s\")", str);

	// Check to see if option fits on one line, and exit early

	/* XXX if (_logic->language() == ENGLISH || textWidth(str) <= MAX_TEXT_WIDTH)*/ {
		strcpy(optionText[0], str);
		return 1;
	}

	abort();

	// Split up multiple line option at closest space character
	// int optionLines = 0;
}

static char *removeStar(char *str) {
	
	// The remove_star function in talk.c uses a static variable, but this
	// modifies the string instead, so the caller should use a copy of the
	// string.

	char *p = strchr(str, '*');
	if (p)
		*p = '\0';

	return str;
}

int16 Talk::selectSentence() {
	// Function TALK_BOB (lines 577-739) in talk.c
	int selectedSentence = 0;

	int scrollX = 0; 	// XXX: global variable
	int startOption = 1;
	int optionLines = 0;
	char optionText[5][MAX_STRING_SIZE];
	int talkZone[5];
	int i;

	// Change NORMAL_INK -> TALK_NORMAL_INK

	_graphics->textCurrentColor(INK_TALK_NORMAL);

	// These bobs are up and down arrows

	BobSlot *arrowBobUp 	= _graphics->bob(ARROW_BOB_UP);
	BobSlot *arrowBobDown = _graphics->bob(ARROW_BOB_DOWN);

	arrowBobUp->x         = 303 + 8 + scrollX;
	arrowBobUp->y         = 150 + 1;
	arrowBobUp->frameNum  = 3;
	arrowBobUp->box.y2    = 199;
	arrowBobUp->active    = false;

	arrowBobDown->x         = 303 + scrollX;
	arrowBobDown->y         = 175;
	arrowBobDown->frameNum  = 4;
	arrowBobDown->box.y2    = 199;
	arrowBobDown->active    = false;

	bool rezone = true;

	while (rezone) {
		rezone = false;

		// Set zones for UP/DOWN text arrows when not English version

		_logic->zoneClearAll(ZONE_PANEL);

		if (_logic->language() != ENGLISH) {
			_logic->zoneSet(ZONE_PANEL, ARROW_ZONE_UP,   MAX_TEXT_WIDTH + 1, 0,  319, 24);
			_logic->zoneSet(ZONE_PANEL, ARROW_ZONE_DOWN, MAX_TEXT_WIDTH + 1, 25, 319, 49);
		}

		_graphics->textClear(151,199);

		int sentenceCount = 0;
		int yOffset = 1;

		for (i = startOption; i <= 4; i++) {
			talkZone[i] = 0;

			if (_talkString[i][0] != '\0') {
				sentenceCount++;

				char temp[MAX_STRING_SIZE];
				strcpy(temp, _talkString[i]);
				optionLines = splitOption(removeStar(temp), optionText);

				if (yOffset < 5) {
					_logic->zoneSet(
							ZONE_PANEL,
							i,
							0,
							yOffset * LINE_HEIGHT - PUSHUP,
							(_logic->language() == ENGLISH) ? 319 : MAX_TEXT_WIDTH,
							(yOffset + optionLines) * LINE_HEIGHT - PUSHUP);
				}

				int j;
				for (j = 0; j < optionLines; j++) {
					if (yOffset < 5) {
						//debug(0, "Draw text '%s'", optionText[j]);
						_graphics->textSet(
								(j == 0) ? 0 : 24, 
								150 - PUSHUP + yOffset * LINE_HEIGHT, 
								optionText[j]);
					}
					yOffset++;
				}

				talkZone[i] = sentenceCount;
			}
		}

		yOffset--;

		// Up and down dialogue arrows

		if (_logic->language() != ENGLISH) {
			arrowBobUp->active    = (startOption > 1);
			arrowBobDown->active  = (yOffset > 4);
		}

		_input->clearKeyVerb();

		if (sentenceCount > 0) {
			int zone = 0;
			int oldZone = 0;

			while (0 == selectedSentence) {

				if (_input->talkQuit())
					break;

				_logic->update();

				zone = _logic->zoneIn(ZONE_PANEL, _input->mousePosX(), _input->mousePosY());

				if (5 == zone || 6 == zone) {
					// XXX Arrow zones
					debug(0, "Arrow zones");
				}
				else {
					if (oldZone != zone) {
						// Changed zone, change text colors
						int y;

						/*debug(0, "Changed zone. oldZone = %i, zone = %i",
								oldZone, zone);*/

						if (zone > 0) {
							for (y = _logic->zoneBox(ZONE_PANEL, zone).y1; y < _logic->zoneBox(ZONE_PANEL, zone).y2; y += 10)
								_graphics->textColor(150 + y, INK_JOE);
						}

						if (oldZone > 0) {
							for (y = _logic->zoneBox(ZONE_PANEL, oldZone).y1; y < _logic->zoneBox(ZONE_PANEL, oldZone).y2; y += 10)
								_graphics->textColor(150 + y, INK_TALK_NORMAL);
						}

						oldZone = zone;
					}

				}

				int mouseButton = _input->mouseButton();
				_input->clearMouseButton();

				if (_input->keyVerb().isDigit()) {
					for (i = 1; i <= 4; i++)
					{
						if (talkZone[i] == _input->keyVerb().digit())
						{
							selectedSentence = i;
							break;
						}
					}

					_input->clearKeyVerb();
				}
				else if (mouseButton) {
					selectedSentence = zone;
					break;
				}

			} // while()
		}
	}


	// XXX Begin debug stuff
	// debug(0, "----- Select a sentence of these -----");
	for (i = 1; i <= 4; i++) {
		if (_talkString[i][0] != '\0') {
			// XXX debug(0, "%i: %s", i, _talkString[i]);
			if (!selectedSentence)
				selectedSentence = i;
		}
	}
	// XXX End debug stuff


	debug(0, "Selected sentence %i", selectedSentence);

	arrowBobUp->active    = false;
	arrowBobDown->active  = false;

	if (selectedSentence > 0) {
		_graphics->textClear(0,198);

		speak(_talkString[selectedSentence], NULL, _joeVoiceFilePrefix[selectedSentence]);
	}
	
	_graphics->textClear(151,151);

	return selectedSentence;
}
	
const Talk::SpeechParameters Talk::_speechParameters[] = {
	{ "JOE",0,1,1,10,2,3,"",0},
	{ "JOE",0,3,3,28,2,3,"",0},
	{ "JOE",0,4,5,38,1,0,"",0},
	{ "JOE",1,1,1,45,-1,0,"",0},
	{ "JOE",1,3,3,28,2,3,"",0},
	{ "JOE",1,4,5,38,1,0,"",0},
	{ "JOE",2,1,1,46,-1,0,"",0},
	{ "JOE",2,3,3,28,2,3,"",0},
	{ "JOE",2,4,5,38,1,0,"",0},
	{ "JOE",3,1,1,47,-1,0,"",0},
	{ "JOE",3,3,3,28,2,3,"",0},
	{ "JOE",3,4,5,38,1,0,"",0},
	{ "JOE",4,1,1,50,-1,0,"",0},
	{ "JOE",4,3,3,28,2,3,"",0},
	{ "JOE",4,4,5,38,1,0,"",0},
	{ "JOE",5,1,2,0,0,0,"",0},
	{ "JOE",5,3,4,0,0,0,"",0},
	{ "JOE",5,4,6,0,0,0,"",0},
	//CR - Change 48,1,0 -> 48,0,1
	{ "JOE",6,1,1,48,0,1,"",0},
	{ "JOE",6,3,3,28,2,3,"",0},
	{ "JOE",6,4,5,38,1,0,"",0},
	//CR - Change 51,1,0 -> 51,0,1
	{ "JOE",7,1,1,51,0,1,"",0},
	{ "JOE",7,3,3,28,2,3,"",0},
	{ "JOE",7,4,5,38,1,0,"",0},
	{ "JOE",8,1,1,26,0,0,"",0},
	{ "JOE",8,3,3,28,2,3,"",0},
	{ "JOE",8,4,5,38,1,0,"",0},
	{ "JOE",9,1,1,29,0,0,"",0},
	{ "JOE",9,3,3,28,0,0,"",0},
	{ "JOE",9,4,5,38,0,0,"",0},
	//     - Look down, sad. 10,12,46
	{ "JOE",10,1,1,12,0,0,"T046,010,010,010,012,012,012,012,012,012,012,012,012,012,012,012,012,012,010,000",0},
	{ "JOE",10,3,3,18,0,0,"",0},
	{ "JOE",10,4,5,44,0,0,"",0},
	{ "JOE",11,1,1,53,-1,0,"",0},
	{ "JOE",11,3,3,28,2,3,"",0},
	{ "JOE",11,4,5,38,1,0,"",0},
	{ "JOE",12,1,1,10,2,3,"",0},
	{ "JOE",12,3,3,28,2,0,"",0},
	{ "JOE",12,4,5,38,1,0,"",0},
	{ "JOE",13,1,1,10,2,3,"T012,013,019,019,019,019,019,019,019,019,019,019,013,010,000",0},
	{ "JOE",13,3,3,28,2,3,"",0},
	{ "JOE",13,4,5,38,1,0,"",0},
	//     - 7/11/94, Make Joe look front
	{ "JOE",14,1,1,16,2,3,"",16},
	{ "JOE",14,3,3,28,2,3,"",0},
	{ "JOE",14,4,5,38,1,0,"",0},

	{ "JOE",15,1,1,58,-1,0,"",0},
	{ "JOE",15,3,3,28,2,3,"",0},
	{ "JOE",15,4,5,38,1,0,"",0},

	{ "JOE",16,1,1,59,-1,0,"",0},
	{ "JOE",16,3,3,28,2,3,"",0},
	{ "JOE",16,4,5,38,1,0,"",0},

	{ "JOE",17,1,1,56,-1,0,"",0},
	{ "JOE",17,3,3,28,2,3,"",0},
	{ "JOE",17,4,5,38,1,0,"",0},

	{ "JOE",18,1,56,16,2,3,"T056,057,057,057,056,056,000",0},
	{ "JOE",18,3,3,28,2,3,"",0},
	{ "JOE",18,4,5,38,1,0,"",0},

	{ "JOE",19,1,54,16,2,3,"T054,055,057,056,000",0},
	{ "JOE",19,3,3,28,2,3,"",0},
	{ "JOE",19,4,5,38,1,0,"",0},

	{ "JOE",20,1,56,16,2,3,"T056,057,055,054,001,000",0},
	{ "JOE",20,3,3,28,2,3,"",0},
	{ "JOE",20,4,5,38,1,0,"",0},

	{ "JOE",21,1,1,60,-1,0,"",0},
	{ "JOE",21,3,3,28,2,3,"",0},
	{ "JOE",21,4,61,38,1,0,"",0},

	{ "JOE",22,1,1,16,2,3,"T063,060,000",0},
	{ "JOE",22,3,3,28,2,3,"",0},
	{ "JOE",22,4,5,38,1,0,"",0},

	{ "JOE",23,1,1,16,2,3,"T060,063,001,000",0},
	{ "JOE",23,3,3,28,2,3,"",0},
	{ "JOE",23,4,5,38,1,0,"",0},

	{ "JOE",24,1,1,47,-2,0,"",0},
	{ "JOE",24,3,3,28,2,3,"",0},
	{ "JOE",24,4,5,38,1,0,"",0},

	{ "RICO",0,0,1,7,1,3,"",7},
	{ "RICO",1,0,1,5,-1,0,"",7},
	{ "RICO",2,0,1,9,0,3,"",7},
	{ "RICO",3,0,1,4,-1,0,"",7},

	{ "EDDY",0,0,14,18,1,3,"",18},
	{ "EDDY",1,0,14,0,0,0,"T016,017,017,016,016,017,017,016,016,017,017,000",18},

	{ "MIKE",0,0,1,2,2,3,"",2},

	{ "LOLA",0,0,30,33,2,3,"",33},
	{ "LOLA",1,0,9,10,2,3,"",33},
	{ "LOLA",2,0,30,33,2,3,"",33},
	{ "LOLA",3,0,32,33,2,3,"",33},
	//CR 2 - 22/2/95,
	{ "LOLA",4,0,8,0,0,0,"",33},
	{ "LOLA",5,0,31,0,0,0,"",0},
	{ "LOLA",6,0,31,0,0,0,"047,048,049,050,000",33},

	{ "LOLA_SHOWER",0,0,7,10,2,3,"",10},
	{ "LOLA_SHOWER",1,0,9,10,2,3,"",10},
	{ "LOLA_SHOWER",2,0,30,33,2,3,"",10},
	{ "LOLA_SHOWER",3,0,32,33,2,3,"",10},
	{ "LOLA_SHOWER",4,0,8,0,0,0,"",0},
	{ "LOLA_SHOWER",5,0,61,0,0,0,"",0},
	{ "LOLA_SHOWER",6,0,64,10,2,3,"",0},
	{ "LOLA_SHOWER",7,0,31,0,0,0,"062,063,064,000",0},

	// { "SECRETARY",0,0,1,12,2,0,"",12},
	// Change Secretary
	{ "SECRETARY",0,0,1,12,2,3,"",12},
	{ "SECRETARY",1,0,1,12,2,0,"",12},
	{ "SECRETARY",2,0,1,12,2,0,"",12},

	// { "SPARKY",0,0,21,23,5,0,"",23},
	// Change Sparky talk code
	{ "SPARKY",0,0,21,23,2,3,"",23},
	{ "SPARKY",1,0,21,22,0,0,"",0},
	{ "SPARKY",2,0,21,22,0,0,"021,042,043,000",0},
	{ "SPARKY",3,0,21,22,0,0,"043,042,021,000",0},
	{ "SPARKY",4,0,43,43,1,0,"",0},
	{ "SPARKY",14,0,21,29,5,0,"",29},

	{ "SPARKY-F",0,0,45,23,5,0,"",23},
	{ "SPARKY-F",1,0,45,47,0,0,"",0},
	{ "SPARKY-F",2,0,45,23,5,0,"045,046,046,045,000",23},
	{ "SPARKY-F",14,0,45,29,5,0,"",29},

	{ "FAYE",0,0,19,35,2,3,"",35},
	{ "FAYE",1,0,19,41,2,3,"",35},
	{ "FAYE",2,0,19,28,-1,0,"",35},
	{ "FAYE",3,0,19,20,0,0,"",0},
	{ "FAYE",4,0,19,27,-1,0,"",35},
	{ "FAYE",5,0,19,29,-1,0,"",35},
	{ "FAYE",6,0,59,35,2,3,"",35},
	{ "FAYE",7,0,19,30,-1,0,"",35},
	{ "FAYE",8,0,19,31,-1,0,"",35},

	{ "BOB",0,0,27,34,2,3,"",34},
	{ "BOB",1,0,27,28,-1,0,"",34},
	{ "BOB",2,0,30,0,0,0,"",0},
	{ "BOB",3,0,31,0,0,0,"",0},
	{ "BOB",4,0,27,61,-1,0,"",34},
	{ "BOB",5,0,27,42,1,0,"",42},

	{ "PYGMY",0,0,20,21,2,6,"",0},
	{ "PYGMY",1,0,20,21,2,6,"020,068,068,068,068,068,068,068,068,020,000",0},
	{ "PYGMY",2,0,20,21,2,6,"T028,029,030,031,031,031,031,030,029,028,035,000",0},
	{ "PYGMY",3,0,20,21,2,6,"T035,036,037,038,037,038,037,038,036,035,000",0},
	{ "PYGMY",4,0,20,21,2,6,"T032,033,032,033,032,033,035,000",0},
	{ "PYGMY",5,0,20,21,2,6,"T023,022,021,022,023,024,025,026,027,026,025,024,023,000",0},
	{ "PYGMY",6,0,20,21,2,6,"T034,034,034,035,000",0},

	{ "WITCH",0,0,39,40,2,6,"",40},
	{ "WITCH",1,0,39,40,2,6,"073,074,000",40},
	{ "WITCH",2,0,39,40,2,6,"074,073,000",40},
	{ "WITCH",3,0,39,40,2,6,"T047,048,049,050,051,000",40},
	{ "WITCH",4,0,39,40,2,6,"T052,053,054,055,056,057,058,057,056,056,056,055,054,053,052,000",40},
	{ "WITCH",5,0,39,40,2,6,"069,070,071,072,073,074,000",40},
	{ "WITCH",6,0,39,40,2,6,"074,073,072,071,070,069,070,000",40},
	{ "WITCH",7,0,39,51,-1,0,"",40},
	{ "WITCH",8,0,39,40,2,6,"T051,050,049,048,047,000",40},

	{ "CHIEF",0,0,1,7,1,7,"",3},
	{ "CHIEF",1,0,1,2,2,6,"062,063,064,065,066,000",0},
	{ "CHIEF",2,0,1,2,2,6,"066,065,064,063,062,000",0},
	{ "CHIEF",3,0,1,17,-1,0,"",3},
	{ "CHIEF",4,0,1,18,-1,0,"",3},
	{ "CHIEF",5,0,1,19,-1,0,"",3},

	{ "NAOMI",0,0,1,2,2,3,"",2},
	{ "NAOMI",1,0,1,2,2,6,"048,049,050,051,052,053,054,055,000",0},
	{ "NAOMI",2,0,1,2,2,6,"055,054,053,052,051,050,049,048,000",0},
	{ "NAOMI",3,0,1,13,-1,0,"",2},
	{ "NAOMI",4,0,1,14,-1,0,"",2},
	{ "NAOMI",5,0,1,10,-1,0,"",2},
	{ "NAOMI",6,0,1,12,-1,0,"",2},
	{ "NAOMI",7,0,1,12,-1,0,"T008,008,008,002,000",2},

	{ "WEDGEWOOD",0,0,8,1,2,0,"",8},
	{ "WEDGEWOOD",1,0,1,1,3,0,"",1},

	// { "BUD",0,0,1,2,4,2,"",2},
	// Change Buds talk code
	{ "BUD",0,0,1,2,3,2,"",2},
	{ "BUD",1,0,1,2,4,2,"T017,018,000",2},
	{ "BUD",2,0,1,21,-1,0,"",2},
	{ "BUD",3,0,1,14,-1,0,"",2},
	{ "BUD",4,0,1,15,-1,0,"",2},
	{ "BUD",5,0,1,20,-1,0,"",2},
	{ "BUD",6,0,1,16,-1,0,"",2},
	{ "BUD",7,0,1,19,-1,0,"",2},
	{ "BUD",8,0,1,17,-1,0,"",2},
	{ "BUD",9,0,1,14,-1,0,"T014,008,008,003,003,008,008,003,003,010,010,012,012,000",2},

	//CR 2 - Change 4,2 -> 2,3, End Frames 8 -> 2
	{ "LOU",0,0,1,2,2,3,"",2},
	{ "LOU",1,0,1,2,4,2,"013,014,015,016,017,018,000",2},
	{ "LOU",2,0,1,2,4,2,"018,017,016,015,014,013,000",2},

	{ "JIMMY",0,0,16,17,2,3,"",17},
	{ "JIMMY",1,0,16,25,-1,0,"",17},
	{ "JIMMY",2,0,16,26,-1,0,"",17},
	{ "JIMMY",3,0,16,27,-1,0,"",17},
	{ "JIMMY",4,0,16,28,-1,0,"",17},
	{ "JIMMY",5,0,16,29,-1,0,"",17},

	{ "TAMMY",0,0,1,2,2,3,"",2},
	{ "TAMMY",1,0,1,2,2,3,"T008,008,009,009,008,008,009,009,008,008,009,009,002,000",2},
	{ "TAMMY",2,0,1,2,2,3,"T002,010,010,010,002,000",2},
	{ "TAMMY",3,0,1,2,2,3,"T011,011,011,011,011,002,000",2},
	{ "TAMMY",4,0,1,2,2,3,"T013,014,015,013,014,015,013,009,001,000",2},

	{ "SKULL",0,0,9,9,4,0,"",0},
	{ "SKULL",1,0,1,9,4,0,"001,002,003,004,005,006,007,008,009,000",0},
	{ "SKULL",2,0,1,9,4,0,"009,008,007,006,005,004,003,002,001,000",0},

	{ "APE",0,0,1,6,7,0,"",6},
	{ "APE",1,0,1,6,7,0,"002,001,000",6},
	{ "APE",2,0,1,6,7,0,"002,003,001,000",6},
	{ "APE",3,0,1,6,7,0,"004,005,004,005,004,001,000",6},
	{ "APE",4,0,1,6,7,0,"001,003,005,004,005,004,001,000",6},

	{ "APE1",0,0,15,16,7,0,"",16},
	{ "APE2",0,0,14,6,7,0,"",6},

	{ "SHOWERAM",0,0,1,2,3,0,"",2},
	{ "SHOWERAM",1,0,1,2,3,0,"026,027,028,029,001,000",2},
	{ "SHOWERAM",2,0,1,2,3,0,"001,029,028,027,026,000",2},

	{ "PRINCESS1",0,0,19,23,2,3,"",23},
	{ "PRINCESS1",1,0,19,41,-1,0,"",23},
	{ "PRINCESS1",2,0,19,42,-1,0,"",23},
	{ "PRINCESS1",3,0,19,45,-1,0,"",23},
	{ "PRINCESS1",4,0,19,40,-1,0,"",23},
	{ "PRINCESS1",5,0,19,45,2,3,"T40,043,044,045,000",45},
	{ "PRINCESS1",6,0,19,45,-1,0,"T041,038,000",38},
	{ "PRINCESS1",7,0,22,0,0,0,"",0},
	{ "PRINCESS1",8,0,19,45,2,3,"T045,044,043,040,039,000",39},

	{ "PRINCESS2",0,0,46,23,2,3,"",23},
	{ "PRINCESS2",1,0,46,29,2,3,"",29},
	{ "PRINCESS2",2,0,46,29,2,3,"T029,036,035,000",35},

	{ "GUARDS",0,0,7,7,0,0,"",7},

	{ "AMGUARD",0,0,19,22,2,3,"",22},

	{ "MAN1",0,0,2,3,2,3,"",3},
	{ "MAN2",0,0,9,10,1,2,"",10},

	{ "DOG",0,0,6,6,1,0,"",0},
	{ "DOG",1,0,6,6,1,0,"010,009,008,000",0},
	{ "DOG",2,0,6,6,1,0,"008,009,010,000",0},

	{ "CHEF",0,0,5,6,2,3,"",6},

	{ "HENRY",0,0,7,9,2,3,"",9},
	{ "HENRY",1,0,7,21,-1,0,"",9},
	{ "HENRY",2,0,7,19,-1,0,"",9},
	{ "HENRY",3,0,7,20,-1,0,"",9},
	{ "HENRY",4,0,8,9,2,3,"",9},
	{ "HENRY",5,0,23,9,-1,0,"",9},
	{ "HENRY",6,0,7,9,2,3,"T019,015,017,017,017,017,017,017,017,015,009,000",9},
	{ "HENRY",7,0,7,9,2,3,"T018,010,000",10},
	{ "HENRY",8,0,7,9,2,3,"T018,016,000",16},
	{ "HENRY",9,0,7,9,2,3,"T018,011,000",11},
	{ "HENRY",10,0,29,33,1,1,"",33},
	{ "HENRY",11,0,7,30,2,0,"",9},
	{ "HENRY",12,0,7,9,2,3,"025,026,000",26},
	{ "HENRY",13,0,7,9,2,3,"027,028,027,028,000",28},
	{ "HENRY",14,0,7,9,2,3,"026,025,007,000",9},

	{ "JOHAN",0,0,1,15,2,3,"",15},
	{ "JOHAN",1,0,1,0,0,0,"T006,007,008,000",15},
	{ "JOHAN",2,0,1,15,2,3,"T002,003,004,005,004,005,004,005,004,005,004,005,004,003,002,000",15},
	{ "JOHAN",3,0,1,8,-1,0,"",15},
	{ "JOHAN",4,0,1,0,0,0,"T008,007,006,001,000",15},

	{ "KLUNK",0,0,1,2,2,3,"",2},
	{ "KLUNK",1,0,1,2,2,3,"019,020,021,022,001,000",2},
	//CR 4 - 22/2/95, Change 017 -> 517
	{ "KLUNK",2,0,1,2,2,3,"001,022,021,020,019,016,517,000",2},
	{ "KLUNK",3,0,1,2,2,3,"T010,011,010,011,010,011,009,000",2},

	{ "FRANK",0,0,13,14,2,3,"",14},
	//CR 2 - 21/2/95, Change from Torso to normal
	{ "FRANK",1,0,13,20,0,1,"",14},
	{ "FRANK",2,0,13,14,2,3,"025,026,027,027,027,026,026,026,027,027,026,026,027,025,013,000",14},
	{ "FRANK",3,0,28,14,2,3,"",14},

	{ "DEATH",0,0,1,2,2,3,"",2},
	{ "DEATH",1,0,1,2,2,3,"013,014,015,016,017,001,000",0},
	{ "DEATH",2,0,1,2,2,3,"001,017,016,015,014,013,000",0},
	//CR 2 - Change 024 -> 524
	{ "DEATH",3,0,1,2,2,3,"T018,019,020,021,021,022,022,020,021,022,020,021,022,023,024,524,000",2},
	{ "DEATH",4,0,1,2,2,3,"T025,026,027,028,028,028,028,028,028,028,028,028,029,035,000",2},
	{ "DEATH",5,0,1,2,2,3,"T030,031,032,033,033,033,033,033,033,033,033,033,034,035,000",2},
	{ "DEATH",6,0,1,2,2,3,"T023,022,020,019,018,001,000",2},

	{ "JASPAR",0,0,1,1,22,0,"026,027,028,029,028,029,028,029,030,023,000",0},
	{ "JASPAR",1,0,1,1,22,0,"023,026,000",0},

	{ "ORACLE",0,0,1,5,3,0,"",0},

	{ "ZOMBIE",0,0,1,5,2,3,"",5},
	{ "ZOMBIE",1,0,1,12,-1,0,"",5},
	{ "ZOMBIE",2,0,1,13,-1,0,"",5},
	{ "ZOMBIE",3,0,1,1,5,5,"",5},

	{ "ZOMBIE2",0,0,14,14,0,0,"",0},
	{ "ZOMBIE3",0,0,18,18,0,0,"",0},

	{ "ANDERSON",0,0,7,8,2,3,"",8},
	{ "ANDERSON",1,0,7,8,1,0,"",8},
	{ "ANDERSON",2,0,7,16,-1,0,"",8},
	{ "ANDERSON",3,0,7,18,-1,0,"",8},
	{ "ANDERSON",4,0,7,19,-1,0,"",8},
	{ "ANDERSON",5,0,7,20,-1,0,"",8},
	{ "ANDERSON",6,0,7,21,1,0,"",8},

	{ "COMPY",0,0,12,12,-1,0,"",0},
	{ "COMPY",1,0,10,10,10,0,"010,011,012,012,013,014,014,000",0},
	{ "COMPY",2,0,10,10,10,0,"014,013,012,000",0},

	{ "DEINO",0,0,13,13,-1,0,"",0},
	{ "DEINO",1,0,9,9,9,0,"009,010,000",0},

	{ "TMPD",0,0,19,22,2,3,"",22},

	{ "IAN",0,0,7,9,2,3,"",9},
	{ "IAN",1,0,8,25,3,0,"",25},
	{ "IAN",2,0,7,21,-1,0,"",9},
	{ "IAN",3,0,7,22,1,0,"",9},
	{ "IAN",4,0,7,22,-1,0,"",9},
	{ "IAN",5,0,7,24,-1,0,"",9},
	{ "IAN",6,0,7,9,2,3,"034,034,034,035,035,036,036,035,035,036,035,036,035,000",9},
	{ "IAN",7,0,7,31,-1,0,"",9},

	{ "FAYE-H",0,0,1,1,4,1,"",1},
	{ "FAYE-H",1,0,1,1,4,1,"007,000",7},
	{ "FAYE-H",2,0,1,1,4,1,"009,010,011,009,001,000",1},
	{ "FAYE-H",3,0,1,1,4,1,"E012,013,000",1},
	{ "FAYE-H",4,0,1,1,4,1,"E015,000",1},
	{ "FAYE-H",5,0,1,1,4,1,"E014,000",1},

	{ "AZURA-H",0,0,1,1,4,1,"",1},
	{ "AZURA-H",1,0,1,1,4,1,"007,000",7},
	{ "AZURA-H",2,0,1,1,4,1,"009,010,011,009,001,000",1},
	{ "AZURA-H",3,0,1,1,4,1,"E012,013,000",1},
	{ "AZURA-H",4,0,1,1,4,1,"E015,000",1},
	{ "AZURA-H",5,0,1,1,4,1,"E014,000",1},

	{ "FRANK-H",0,0,1,1,4,1,"",1},
	{ "FRANK-H",1,0,1,1,4,1,"E009,000",1},
	{ "FRANK-H",2,0,1,1,4,1,"E007,000",1},
	{ "FRANK-H",3,0,1,1,4,1,"010,011,012,013,014,015,010,000",1},

	{ "JOE-E",0,0,1,2,4,1,"",2},
	{ "JOE-E",6,0,1,2,4,1,"008,009,008,002,000",2},

	{ "AZURA-E",0,0,1,1,5,1,"",1},
	{ "AZURA-E",1,0,1,1,5,1,"009,010,009,008,000",1},

	{ "FAYE-E",0,0,1,4,4,1,"",1},
	{ "FAYE-E",1,0,1,4,4,1,"002,003,002,001,000",1},

	{ "ANDSON-E",0,0,1,3,4,1,"",1},
	{ "ANDSON-E",1,0,1,3,4,1,"002,001,000",1},

//#ifdef __DOS__

	{ "JOE-H",0,0,1,1,4,4,"",1},
	{ "JOE-H",1,0,1,1,2,3,"012,013,014,000",14},
	{ "JOE-H",2,0,1,1,2,3,"010,011,000",11},
	{ "JOE-H",3,0,1,1,2,3,"014,013,012,001,000",1},
	{ "JOE-H",4,0,1,13,1,0,"",13},

	{ "RITA-H",0,0,7,1,2,3,"",1},
	{ "RITA-H",1,0,7,0,0,0,"009,010,011,012,013,000",13},
	{ "RITA-H",2,0,7,0,0,0,"014,015,016,000",16},
	{ "RITA-H",3,0,7,0,0,0,"013,012,011,010,000",10},
	{ "RITA-H",4,0,7,0,0,0,"009,007,008,007,009,000",9},
	{ "RITA-H",5,0,7,0,0,0,"016,015,014,000",14},

	{ "RITA",0,0,1,4,2,3,"",4},
	{ "RITA",1,0,2,4,2,3,"",4},

	{ "SPARKY-H",0,0,1,1,2,3,"",1},

	{ "HUGH",0,0,1,1,2,3,"",1},
	{ "HUGH",1,0,7,7,2,3,"",7},

	// New CD-Rom intro persons
	// *01 Blink
	{ "X2_JOE",0,0,1,1,2,3,"",1},
	{ "X2_JOE",1,0,1,1,2,3,"001,007,008,008,007,001,000",1},

	// *01 Blink
	{ "X2_RITA",0,0,1,1,2,3,"",1},
	{ "X2_RITA",1,0,1,1,2,3,"001,007,008,008,007,001,000",1},

	// *01 Smile, *02 Blink, *03 Raise Eyebrow, *04 Anger, *05 Surprise
	{ "X3_RITA",0,0,1,1,4,1,"",1},
	{ "X3_RITA",1,0,1,1,4,1,"007,000",7},
	{ "X3_RITA",2,0,1,1,4,1,"009,010,011,009,001,000",1},
	{ "X3_RITA",3,0,1,1,4,1,"E012,013,000",1},
	{ "X3_RITA",4,0,1,1,4,1,"E015,000",1},
	{ "X3_RITA",5,0,1,1,4,1,"E014,000",1},

	// *01 Talk to Sparky, *02 Turn to Sparky, *03 Turn to Rita, *04 Close Eyes
	{ "X4_JOE",0,0,1,1,3,4,"",1},
	{ "X4_JOE",1,0,1,13,2,3,"",13},
	{ "X4_JOE",2,0,1,1,3,4,"009,010,011,012,013,000",13},
	{ "X4_JOE",3,0,1,1,3,4,"012,011,010,009,000",9},
	{ "X4_JOE",4,0,1,1,3,4,"001,019,000",19},

	// *01 Talk to Sparky, *02 Turn to Sparky, *03 Turn to Joe, *04 Close Eyes
	{ "X4_RITA",0,0,1,1,0,1,"",1},
	{ "X4_RITA",1,0,1,7,0,1,"",7},
	{ "X4_RITA",2,0,1,1,3,4,"004,005,006,006,006,006,007,000",7},
	{ "X4_RITA",3,0,1,1,3,4,"005,004,001,000",1},
	{ "X4_RITA",4,0,1,1,3,4,"001,003,000",3},

	// *01 Blink, *02 Look Right
	{ "X5_SPARKY",0,0,1,1,2,3,"",1},
	{ "X5_SPARKY",1,0,1,1,2,3,"001,010,011,011,001,000",1},
	{ "X5_SPARKY",2,0,1,1,2,3,"001,007,008,009,000",9},

	// *01 Eyes Bulge Once, *02 Eyes Bulge Big Time
	{ "X6_HUGH",0,0,1,1,2,3,"",1},
	{ "X6_HUGH",1,0,1,1,2,3,"007,007,007,007,,001,000",1},
	{ "X6_HUGH",2,0,1,1,2,3,"008,008,008,008,008,009,009,008,008,008,009,008,000",8},

	// *01 Talk to Sparky, Rub Head
	{ "X10_JOE",0,0,1,2,2,3,"",2},
	{ "X10_JOE",1,0,1,8,2,3,"",8},
	//CR 2 - 21/2/95, Cut down rub head anim
	{ "X10_JOE",2,0,1,2,2,3,"014,014,014,015,015,014,014,015,015,000",2},

	{ "X10_RITA",0,0,1,2,2,3,"",2},

	{ "X11_JOE",0,0,1,2,0,1,"",2},

	// *01 Look at Joe
	{ "X11_RITA",0,0,1,2,0,1,"",2},
	{ "X11_RITA",1,0,1,2,1,0,"003,004,000",4},

//#endif

	{ "*",0,0,0,0,0,0,"",0}
};


} // End of namespace Queen
