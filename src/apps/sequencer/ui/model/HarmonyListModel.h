#pragma once

#include "ListModel.h"

#include "model/NoteSequence.h"
#include "model/Model.h"
#include "model/Track.h"
#include "Config.h"

class HarmonyListModel : public ListModel {
public:
    enum Item {
        HarmonyRole,
        MasterTrack,
        HarmonyScale,
        HarmonyInversion,
        HarmonyVoicing,
        HarmonyTranspose,
        Last
    };

    HarmonyListModel() : _sequence(nullptr), _model(nullptr) {}

    void setSequence(NoteSequence *sequence) {
        _sequence = sequence;
    }

    void setModel(Model *model) {
        _model = model;
    }

    virtual int rows() const override {
        if (!_sequence) return 0;

        auto role = _sequence->harmonyRole();

        switch (role) {
        case NoteSequence::HarmonyOff:
            return 1; // Role only
        case NoteSequence::HarmonyMaster:
            return 4; // Role, Mode, Inversion, Voicing
        default: // Follower roles
            return 4; // Role, MasterTrack, Mode, Transpose
        }
    }

    virtual int columns() const override {
        return 2;
    }

    virtual void cell(int row, int column, StringBuilder &str) const override {
        if (!_sequence) return;

        Item item = rowToItem(row);
        if (item == Last) return;

        if (column == 0) {
            formatName(item, str);
        } else if (column == 1) {
            formatValue(item, str);
        }
    }

    virtual void edit(int row, int column, int value, bool shift) override {
        if (!_sequence || column != 1) return;

        Item item = rowToItem(row);
        if (item == Last) return;

        int count = indexedCount(item);
        if (count > 0) {
            // For indexed values, cycle through the options
            int current = indexed(item);
            if (current >= 0) {
                int next = (current + value) % count;
                // Handle negative values properly
                if (next < 0) next += count;
                setIndexed(item, next);
            }
        } else {
            // For non-indexed values, use the original editValue method
            editValue(item, value, shift);
        }
    }

    virtual int indexedCount(int row) const override {
        if (!_sequence) return 0;
        return indexedCount(rowToItem(row));
    }

    int indexedCount(Item item) const {
        if (!_sequence) return 0;

        switch (item) {
        case HarmonyRole:
            return 6; // Off, Master, FollowerRoot, Follower3rd, Follower5th, Follower7th
        case HarmonyScale:
            return 7; // Ionian, Dorian, Phrygian, Lydian, Mixolydian, Aeolian, Locrian
        case HarmonyInversion:
            return 4; // Root, 1st, 2nd, 3rd
        case HarmonyVoicing:
            return 4; // Close, Drop2, Drop3, Spread
        default:
            return 0;
        }
    }

    virtual int indexed(int row) const override {
        if (!_sequence) return -1;
        return indexed(rowToItem(row));
    }

    int indexed(Item item) const {
        if (!_sequence) return -1;

        switch (item) {
        case HarmonyRole:
            return static_cast<int>(_sequence->harmonyRole());
        case HarmonyScale:
            return _sequence->harmonyScale();
        case HarmonyInversion:
            return _sequence->harmonyInversion();
        case HarmonyVoicing:
            return _sequence->harmonyVoicing();
        default:
            return -1;
        }
    }

    virtual void setIndexed(int row, int index) override {
        if (!_sequence || index < 0) return;
        setIndexed(rowToItem(row), index);
    }

    void setIndexed(Item item, int index) {
        if (!_sequence || index < 0) return;

        if (index >= indexedCount(item)) return;

        switch (item) {
        case HarmonyRole: {
            auto newRole = static_cast<NoteSequence::HarmonyRole>(index);

            // ===== BECOMING FOLLOWER =====
            if (newRole >= NoteSequence::HarmonyFollowerRoot) {
                // Find valid master
                int proposedMaster = findValidMaster();
                int selfIndex = _sequence->trackIndex();

                // Validate master
                if (proposedMaster == selfIndex) {
                    // Can't follow self - block change
                    return;
                }

                if (!isValidNoteTrack(proposedMaster)) {
                    // No valid Note tracks available - block change
                    return;
                }

                if (wouldCreateCycle(proposedMaster)) {
                    // Would create circular dependency - block change
                    return;
                }

                // Apply changes
                _sequence->setMasterTrackIndex(proposedMaster);
                _sequence->setHarmonyTranspose(0); // Reset transpose

                // Keep follower's own Mode (engine uses follower harmonyScale)
                // Inversion/Voicing come from master sequence/step in the engine
            }

            // ===== BECOMING MASTER =====
            else if (newRole == NoteSequence::HarmonyMaster) {
                // Set safe defaults if never configured
                if (_sequence->harmonyScale() > 6) {
                    _sequence->setHarmonyScale(0); // Ionian
                }
                if (_sequence->harmonyInversion() > 3) {
                    _sequence->setHarmonyInversion(0); // Root position
                }
                if (_sequence->harmonyVoicing() > 3) {
                    _sequence->setHarmonyVoicing(0); // Close voicing
                }

                // Force transpose to 0 (unused by master)
                _sequence->setHarmonyTranspose(0);
            }

            // ===== BECOMING OFF =====
            // No special handling needed

            // Set new role
            _sequence->setHarmonyRole(newRole);
            break;
        }
        case HarmonyScale:
            _sequence->setHarmonyScale(index);
            break;
        case HarmonyInversion:
            _sequence->setHarmonyInversion(index);
            break;
        case HarmonyVoicing:
            _sequence->setHarmonyVoicing(index);
            break;
        default:
            break;
        }
    }

private:
    static const char *itemName(Item item) {
        switch (item) {
        case HarmonyRole:      return "ROLE";
        case MasterTrack:      return "MASTER";
        case HarmonyScale:     return "MODE";
        case HarmonyInversion: return "INVERSION";
        case HarmonyVoicing:   return "VOICING";
        case HarmonyTranspose: return "CH-TRNSP";
        case Last:             break;
        }
        return nullptr;
    }

    void formatName(Item item, StringBuilder &str) const {
        str(itemName(item));
    }

    void formatValue(Item item, StringBuilder &str) const {
        if (!_sequence) return;

        switch (item) {
        case HarmonyRole: {
            auto role = _sequence->harmonyRole();
            switch (role) {
            case NoteSequence::HarmonyOff:          str("OFF"); break;
            case NoteSequence::HarmonyMaster:       str("MASTER"); break;
            case NoteSequence::HarmonyFollowerRoot: str("ROOT"); break;
            case NoteSequence::HarmonyFollower3rd:  str("3RD"); break;
            case NoteSequence::HarmonyFollower5th:  str("5TH"); break;
            case NoteSequence::HarmonyFollower7th:  str("7TH"); break;
            }
            break;
        }
        case MasterTrack:
            str("T%d", _sequence->masterTrackIndex() + 1); // Display as 1-8
            break;
        case HarmonyScale: {
            int scale = _sequence->harmonyScale();
            switch (scale) {
            case 0: str("IONIAN"); break;
            case 1: str("DORIAN"); break;
            case 2: str("PHRYGN"); break;
            case 3: str("LYDIAN"); break;
            case 4: str("MIXOLY"); break;
            case 5: str("AEOLIN"); break;
            case 6: str("LOCRIN"); break;
            default: str("---"); break;
            }
            break;
        }
        case HarmonyInversion: {
            int inversion = _sequence->harmonyInversion();
            switch (inversion) {
            case 0: str("ROOT"); break;
            case 1: str("1ST"); break;
            case 2: str("2ND"); break;
            case 3: str("3RD"); break;
            default: str("---"); break;
            }
            break;
        }
        case HarmonyVoicing: {
            int voicing = _sequence->harmonyVoicing();
            switch (voicing) {
            case 0: str("CLOSE"); break;
            case 1: str("DROP2"); break;
            case 2: str("DROP3"); break;
            case 3: str("SPREAD"); break;
            default: str("---"); break;
            }
            break;
        }
        case HarmonyTranspose:
            str("%+d", _sequence->harmonyTranspose());
            break;
        case Last:
            break;
        }
    }

    void editValue(Item item, int value, bool shift) {
        if (!_sequence) return;

        switch (item) {
        case MasterTrack: {
            int currentMaster = _sequence->masterTrackIndex();
            int direction = value >= 0 ? 1 : -1;
            int proposedMaster = findNextValidMaster(currentMaster, direction);

            if (proposedMaster != currentMaster) {
                _sequence->setMasterTrackIndex(proposedMaster);
            }
            break;
        }
        case HarmonyTranspose:
            _sequence->setHarmonyTranspose(
                clamp(_sequence->harmonyTranspose() + value, -24, 24));
            break;
        default:
            break;
        }
    }

    int clamp(int value, int min, int max) const {
        if (value < min) return min;
        if (value > max) return max;
        return value;
    }

    // Map physical row to logical item based on current role
    Item rowToItem(int row) const {
        if (!_sequence || row < 0) return Last;

        auto role = _sequence->harmonyRole();

        // Row 0 is always Role
        if (row == 0) return HarmonyRole;

        // Role-specific mapping
        if (role == NoteSequence::HarmonyMaster) {
            switch (row) {
            case 1: return HarmonyScale;
            case 2: return HarmonyInversion;
            case 3: return HarmonyVoicing;
            default: return Last;
            }
        } else if (role >= NoteSequence::HarmonyFollowerRoot) {
            switch (row) {
            case 1: return MasterTrack;
            case 2: return HarmonyScale;
            case 3: return HarmonyTranspose;
            default: return Last;
            }
        }

        return Last;
    }

    // Check if a track index points to a valid Note track
    bool isValidNoteTrack(int trackIndex) const {
        if (!_model || trackIndex < 0 || trackIndex >= CONFIG_TRACK_COUNT) {
            return false;
        }
        return _model->project().track(trackIndex).trackMode() == Track::TrackMode::Note;
    }

    // Find first valid Note track that isn't self
    int findValidMaster() const {
        if (!_sequence || !_model) return 0;

        int selfIndex = _sequence->trackIndex();

        // Try current masterTrackIndex first
        int current = _sequence->masterTrackIndex();
        if (current != selfIndex && isValidNoteTrack(current)) {
            return current;
        }

        // Find first valid Note track
        for (int i = 0; i < CONFIG_TRACK_COUNT; i++) {
            if (i != selfIndex && isValidNoteTrack(i)) {
                return i;
            }
        }

        // Fallback to 0 (will fail validation)
        return 0;
    }

    // Check if changing to this master would create a cycle
    bool wouldCreateCycle(int proposedMasterIdx) const {
        if (!_sequence || !_model || !isValidNoteTrack(proposedMasterIdx)) {
            return false;
        }

        int selfIndex = _sequence->trackIndex();
        int current = proposedMasterIdx;

        // Walk the master chain until it ends or loops back to self
        for (int depth = 0; depth < CONFIG_TRACK_COUNT; ++depth) {
            if (current == selfIndex) {
                return true; // Cycle detected
            }

            const auto &track = _model->project().track(current);
            if (track.trackMode() != Track::TrackMode::Note) {
                return false; // Chain ends at non-Note track
            }

            const auto &seq = track.noteTrack().sequence(0);
            if (seq.harmonyRole() < NoteSequence::HarmonyFollowerRoot) {
                return false; // Chain ends at non-follower
            }

            current = seq.masterTrackIndex();
        }

        return false; // No cycle within track count
    }

    // Find next valid master, scanning in the edit direction
    int findNextValidMaster(int startIndex, int direction) const {
        if (!_sequence || !_model) return startIndex;

        int selfIndex = _sequence->trackIndex();

        for (int i = 0; i < CONFIG_TRACK_COUNT; ++i) {
            int candidate = (startIndex + direction + CONFIG_TRACK_COUNT) % CONFIG_TRACK_COUNT;
            startIndex = candidate;

            if (candidate == selfIndex) continue;
            if (!isValidNoteTrack(candidate)) continue;
            if (wouldCreateCycle(candidate)) continue;

            return candidate;
        }

        return startIndex; // No valid master found, return current
    }

    NoteSequence *_sequence;
    Model *_model;
};
