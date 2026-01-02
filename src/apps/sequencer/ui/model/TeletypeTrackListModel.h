#pragma once

#include "RoutableListModel.h"
#include "model/TeletypeTrack.h"

class TeletypeTrackListModel : public RoutableListModel {
public:
    void setTrack(TeletypeTrack &track) {
        _track = &track;
    }

    virtual int rows() const override {
        return _track ? Last : 0;
    }

    virtual int columns() const override {
        return 2;
    }

    virtual void cell(int row, int column, StringBuilder &str) const override {
        if (column == 0) {
            formatName(Item(row), str);
        } else if (column == 1) {
            formatValue(Item(row), str);
        }
    }

    virtual void edit(int row, int column, int value, bool shift) override {
        if (column == 1) {
            editValue(Item(row), value, shift);
        }
    }

    virtual Routing::Target routingTarget(int row) const override {
        (void)row;
        return Routing::Target::None;
    }

private:
    enum Item {
        TimeBase,
        ClockDivisor,
        ClockMultiplier,
        TriggerIn1,
        TriggerIn2,
        TriggerIn3,
        TriggerIn4,
        CvIn,
        CvParam,
        TriggerOutA,
        TriggerOutB,
        TriggerOutC,
        TriggerOutD,
        CvOut1,
        CvOut2,
        CvOut3,
        CvOut4,
        Last
    };

    static const char *itemName(Item item) {
        switch (item) {
        case TimeBase:    return "TIMEBASE";
        case ClockDivisor: return "CLK.DIV";
        case ClockMultiplier: return "CLK.MULT";
        case TriggerIn1:  return "TI-TR1";
        case TriggerIn2:  return "TI-TR2";
        case TriggerIn3:  return "TI-TR3";
        case TriggerIn4:  return "TI-TR4";
        case CvIn:        return "TI-IN";
        case CvParam:     return "TI-PARAM";
        case TriggerOutA: return "TO-TRA";
        case TriggerOutB: return "TO-TRB";
        case TriggerOutC: return "TO-TRC";
        case TriggerOutD: return "TO-TRD";
        case CvOut1:      return "TO-CV1";
        case CvOut2:      return "TO-CV2";
        case CvOut3:      return "TO-CV3";
        case CvOut4:      return "TO-CV4";
        case Last:        break;
        }
        return nullptr;
    }

    void formatName(Item item, StringBuilder &str) const {
        str(itemName(item));
    }

    void formatValue(Item item, StringBuilder &str) const {
        switch (item) {
        case TimeBase:
            _track->printTimeBase(str);
            break;
        case ClockDivisor:
            _track->printClockDivisor(str);
            break;
        case ClockMultiplier:
            _track->printClockMultiplier(str);
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
        case TriggerOutA:
        case TriggerOutB:
        case TriggerOutC:
        case TriggerOutD: {
            int index = int(item) - int(TriggerOutA);
            _track->printTriggerOutputDest(index, str);
            break;
        }
        case CvOut1:
        case CvOut2:
        case CvOut3:
        case CvOut4: {
            int index = int(item) - int(CvOut1);
            _track->printCvOutputDest(index, str);
            break;
        }
        case Last:
            break;
        }
    }

    void editValue(Item item, int value, bool shift) {
        switch (item) {
        case TimeBase:
            _track->editTimeBase(value, shift);
            break;
        case ClockDivisor:
            _track->editClockDivisor(value, shift);
            break;
        case ClockMultiplier:
            _track->editClockMultiplier(value, shift);
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
        case Last:
            break;
        }
    }

    TeletypeTrack *_track = nullptr;
};
