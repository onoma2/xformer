#pragma once

#include "RoutableListModel.h"
#include "model/Project.h"
#include "model/TeletypeTrack.h"

class TeletypeTrackListModel : public RoutableListModel {
public:
    void setTrack(TeletypeTrack &track, Project &project, int trackIndex) {
        _track = &track;
        _project = &project;
        _trackIndex = trackIndex;
    }

    virtual int rows() const override {
        return _track ? rowCount() : 0;
    }

    virtual int columns() const override {
        return 2;
    }

    virtual void cell(int row, int column, StringBuilder &str) const override {
        Item item = itemForRow(row);
        if (column == 0) {
            formatName(item, str);
        } else if (column == 1) {
            formatValue(item, str);
        }
    }

    virtual void edit(int row, int column, int value, bool shift) override {
        if (column == 1) {
            editValue(itemForRow(row), value, shift);
        }
    }

    virtual Routing::Target routingTarget(int row) const override {
        (void)row;
        return Routing::Target::None;
    }

private:
    enum Item {
        MidiSource,
        TimeBase,
        ClockDivisor,
        ClockMultiplier,
        BootScript,
        TriggerIn1,
        TriggerIn2,
        TriggerIn3,
        TriggerIn4,
        CvIn,
        CvParam,
        CvX,
        CvY,
        CvZ,
        TriggerOutA,
        TriggerOutB,
        TriggerOutC,
        TriggerOutD,
        CvOut1,
        CvOut2,
        CvOut3,
        CvOut4,
        CvQuantize1,
        CvQuantize2,
        CvQuantize3,
        CvQuantize4,
        CvRoot1,
        CvRoot2,
        CvRoot3,
        CvRoot4,
        Cv1Range,
        Cv1Offset,
        Cv2Range,
        Cv2Offset,
        Cv3Range,
        Cv3Offset,
        Cv4Range,
        Cv4Offset,
        Last
    };

    static const char *itemName(Item item) {
        switch (item) {
        case MidiSource:  return "MIDI SRC";
        case TimeBase:    return "TIMEBASE";
        case ClockDivisor: return "CLK.DIV";
        case ClockMultiplier: return "CLK.MULT";
        case BootScript:  return "INIT SCR";
        case TriggerIn1:  return "TI-TR1";
        case TriggerIn2:  return "TI-TR2";
        case TriggerIn3:  return "TI-TR3";
        case TriggerIn4:  return "TI-TR4";
        case CvIn:        return "TI-IN";
        case CvParam:     return "TI-PARAM";
        case CvX:         return "TI-X";
        case CvY:         return "TI-Y";
        case CvZ:         return "TI-Z";
        case TriggerOutA: return "TO-TRA";
        case TriggerOutB: return "TO-TRB";
        case TriggerOutC: return "TO-TRC";
        case TriggerOutD: return "TO-TRD";
        case CvOut1:      return "TO-CV1";
        case CvOut2:      return "TO-CV2";
        case CvOut3:      return "TO-CV3";
        case CvOut4:      return "TO-CV4";
        case CvQuantize1: return "QTO-CV1";
        case CvQuantize2: return "QTO-CV2";
        case CvQuantize3: return "QTO-CV3";
        case CvQuantize4: return "QTO-CV4";
        case CvRoot1:     return "RTO-CV1";
        case CvRoot2:     return "RTO-CV2";
        case CvRoot3:     return "RTO-CV3";
        case CvRoot4:     return "RTO-CV4";
        case Cv1Range:    return "CV1 RNG";
        case Cv1Offset:   return "CV1 OFF";
        case Cv2Range:    return "CV2 RNG";
        case Cv2Offset:   return "CV2 OFF";
        case Cv3Range:    return "CV3 RNG";
        case Cv3Offset:   return "CV3 OFF";
        case Cv4Range:    return "CV4 RNG";
        case Cv4Offset:   return "CV4 OFF";
        case Last:        break;
        }
        return nullptr;
    }

    void formatName(Item item, StringBuilder &str) const {
        str(itemName(item));
    }

    void formatValue(Item item, StringBuilder &str) const {
        switch (item) {
        case MidiSource:
            _track->midiSource().print(str);
            break;
        case TimeBase:
            _track->printTimeBase(str);
            break;
        case ClockDivisor:
            _track->printClockDivisor(str);
            break;
        case ClockMultiplier:
            _track->printClockMultiplier(str);
            break;
        case BootScript:
            str("S%d", _track->bootScriptIndex() + 1);
            break;
        case TriggerIn1:
        case TriggerIn2:
        case TriggerIn3:
        case TriggerIn4: {
            int index = int(item) - int(TriggerIn1);
            _track->printTriggerInputSource(index, str);
            break;
        }
        case CvIn:
            _track->printCvInSource(str);
            break;
        case CvParam:
            _track->printCvParamSource(str);
            break;
        case CvX:
            _track->printCvXSource(str);
            break;
        case CvY:
            _track->printCvYSource(str);
            break;
        case CvZ:
            _track->printCvZSource(str);
            break;
        case TriggerOutA:
        case TriggerOutB:
        case TriggerOutC:
        case TriggerOutD: {
            int index = int(item) - int(TriggerOutA);
            _track->printTriggerOutputDest(index, str);
            if (_project) {
                int gateIdx = int(_track->triggerOutputDest(index));
                if (_project->gateOutputTrack(gateIdx) != _trackIndex) {
                    str(" !");
                }
            }
            break;
        }
        case CvOut1:
        case CvOut2:
        case CvOut3:
        case CvOut4: {
            int index = int(item) - int(CvOut1);
            _track->printCvOutputDest(index, str);
            if (_project) {
                int cvIdx = int(_track->cvOutputDest(index));
                if (_project->cvOutputTrack(cvIdx) != _trackIndex) {
                    str(" !");
                }
            }
            break;
        }
        case CvQuantize1:
        case CvQuantize2:
        case CvQuantize3:
        case CvQuantize4: {
            int index = int(item) - int(CvQuantize1);
            _track->printCvOutputQuantizeScale(index, str);
            break;
        }
        case CvRoot1:
        case CvRoot2:
        case CvRoot3:
        case CvRoot4: {
            int index = int(item) - int(CvRoot1);
            _track->printCvOutputRootNote(index, str);
            break;
        }
        case Cv1Range:
        case Cv2Range:
        case Cv3Range:
        case Cv4Range: {
            int index = (int(item) - int(Cv1Range)) / 2;
            _track->printCvOutputRange(index, str);
            break;
        }
        case Cv1Offset:
        case Cv2Offset:
        case Cv3Offset:
        case Cv4Offset: {
            int index = (int(item) - int(Cv1Offset)) / 2;
            _track->printCvOutputOffset(index, str);
            break;
        }
        case Last:
            break;
        }
    }

    void editValue(Item item, int value, bool shift) {
        switch (item) {
        case MidiSource:
            _track->midiSource().edit(value, shift);
            _track->syncToActiveSlot();
            break;
        case TimeBase:
            _track->editTimeBase(value, shift);
            break;
        case ClockDivisor:
            _track->editClockDivisor(value, shift);
            break;
        case ClockMultiplier:
            _track->editClockMultiplier(value, shift);
            break;
        case BootScript:
            _track->setBootScriptIndex(_track->bootScriptIndex() + value);
            break;
        case TriggerIn1:
        case TriggerIn2:
        case TriggerIn3:
        case TriggerIn4: {
            int index = int(item) - int(TriggerIn1);
            _track->editTriggerInputSource(index, value, shift);
            break;
        }
        case CvIn:
            _track->editCvInSource(value, shift);
            break;
        case CvParam:
            _track->editCvParamSource(value, shift);
            break;
        case CvX:
            _track->editCvXSource(value, shift);
            break;
        case CvY:
            _track->editCvYSource(value, shift);
            break;
        case CvZ:
            _track->editCvZSource(value, shift);
            break;
        case TriggerOutA:
        case TriggerOutB:
        case TriggerOutC:
        case TriggerOutD: {
            int index = int(item) - int(TriggerOutA);
            _track->editTriggerOutputDest(index, value, shift);
            break;
        }
        case CvOut1:
        case CvOut2:
        case CvOut3:
        case CvOut4: {
            int index = int(item) - int(CvOut1);
            _track->editCvOutputDest(index, value, shift);
            break;
        }
        case CvQuantize1:
        case CvQuantize2:
        case CvQuantize3:
        case CvQuantize4: {
            int index = int(item) - int(CvQuantize1);
            _track->editCvOutputQuantizeScale(index, value, shift);
            break;
        }
        case CvRoot1:
        case CvRoot2:
        case CvRoot3:
        case CvRoot4: {
            int index = int(item) - int(CvRoot1);
            _track->editCvOutputRootNote(index, value, shift);
            break;
        }
        case Cv1Range:
        case Cv2Range:
        case Cv3Range:
        case Cv4Range: {
            int index = (int(item) - int(Cv1Range)) / 2;
            _track->editCvOutputRange(index, value, shift);
            break;
        }
        case Cv1Offset:
        case Cv2Offset:
        case Cv3Offset:
        case Cv4Offset: {
            int index = (int(item) - int(Cv1Offset)) / 2;
            _track->editCvOutputOffset(index, value, shift);
            break;
        }
        case Last:
            break;
        }
    }

    bool showClockParams() const {
        return _track && _track->timeBase() == TeletypeTrack::TimeBase::Clock;
    }

    int rowCount() const {
        return showClockParams() ? Last : Last - 2;
    }

    Item itemForRow(int row) const {
        if (!showClockParams() && row >= int(ClockDivisor)) {
            row += 2;
        }
        return Item(row);
    }

    TeletypeTrack *_track = nullptr;
    Project *_project = nullptr;
    int _trackIndex = -1;
};
