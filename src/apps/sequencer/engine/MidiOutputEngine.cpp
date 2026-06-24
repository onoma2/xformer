#include "MidiOutputEngine.h"
#include "Engine.h"
#include "WallClock.h"
#include "MidiTuning.h"

#include "model/Model.h"
#include "model/MidiOutput.h"

#include "core/midi/MidiMessage.h"

MidiOutputEngine::MidiOutputEngine(Engine &engine, Model &model):
    _engine(engine),
    _midiOutput(model.project().midiOutput())
{
}

void MidiOutputEngine::reset() {
    for (int outputIndex = 0; outputIndex < CONFIG_MIDI_OUTPUT_COUNT; ++outputIndex) {
        resetOutput(outputIndex);
    }
}

void MidiOutputEngine::update(bool forceSendCC) {
    // determine the rate for sending MIDI CC events:
    // 3250 bytes per second
    // 3 bytes per message
    // 16 outputs
    // gives roughly 67 events per second per track
    // be conservative and make this 50 events per second
    bool sendCC = forceSendCC;
    uint32_t nowUs = WallClock().now();
    if (nowUs - _lastSendCCUs >= 20000) { // 50 events/s (20 ms in µs), wrap-safe
        sendCC = true;
        _lastSendCCUs = nowUs;
    }

    for (int outputIndex = 0; outputIndex < CONFIG_MIDI_OUTPUT_COUNT; ++outputIndex) {
        const auto &output = _midiOutput.output(outputIndex);
        auto &outputState = _outputStates[outputIndex];

        // check if event or target has changed
        if (outputState.event != output.event() || outputState.target != output.target()) {
            resetOutput(outputIndex);
            outputState.event = output.event();
            outputState.target = output.target();
        }

        if (!outputState.requests) {
            continue;
        }

        MidiPort port = MidiPort(output.target().port());
        int channel = output.target().channel();

        // send slide requests
        if (outputState.hasRequest(OutputState::Slide)) {
            sendMidi(port, MidiMessage::makeControlChange(channel, 65, outputState.slide ? 127 : 0));
            outputState.clearRequest(OutputState::Slide);
        }

        // send note requests
        if (outputState.hasRequest(OutputState::NoteOn | OutputState::NoteOff)) {
            int note = int(output.noteSource()) <= int(MidiOutput::Output::NoteSource::LastTrack) ?
                outputState.note :
                int(output.noteSource()) - int(MidiOutput::Output::NoteSource::FirstNote);

            int velocity = int(output.velocitySource()) <= int(MidiOutput::Output::VelocitySource::LastTrack) ?
                outputState.velocity :
                int(output.velocitySource()) - int(MidiOutput::Output::VelocitySource::FirstVelocity);

            bool mt = output.microtune();
            bool bendChanged = mt && outputState.signedBend != outputState.activeBend;

            if (mt && bendChanged && outputState.hasRequest(OutputState::NoteOn) &&
                    note == outputState.activeNote && outputState.activeNote != -1) {
                // same nearest note, new bend: off -> bend -> on (one MIDI number can't overlap itself)
                sendMidi(port, MidiMessage::makeNoteOff(channel, outputState.activeNote));
                sendMidi(port, MidiMessage::makePitchBend(channel, outputState.signedBend));
                sendMidi(port, MidiMessage::makeNoteOn(channel, note, velocity));
                outputState.activeNote = note;
                outputState.activeBend = outputState.signedBend;
                outputState.clearRequest(OutputState::NoteOn | OutputState::NoteOff);
            } else {
                if (outputState.hasRequest(OutputState::NoteOn) && outputState.hasRequest(OutputState::NoteOff) && note == outputState.activeNote) {
                    outputState.clearRequest(OutputState::NoteOn | OutputState::NoteOff);
                }
                if (outputState.hasRequest(OutputState::NoteOn) && outputState.activeNote != note) {
                    if (mt) {
                        sendMidi(port, MidiMessage::makePitchBend(channel, outputState.signedBend));
                    }
                    sendMidi(port, MidiMessage::makeNoteOn(channel, note, velocity));
                }
                if (outputState.hasRequest(OutputState::NoteOff) && outputState.activeNote != -1) {
                    sendMidi(port, MidiMessage::makeNoteOff(channel, outputState.activeNote));
                    outputState.activeNote = -1;
                }
                if (outputState.hasRequest(OutputState::NoteOn) && outputState.activeNote != -1 && outputState.activeNote != note) {
                    sendMidi(port, MidiMessage::makeNoteOff(channel, outputState.activeNote));
                    outputState.activeNote = -1;
                }
                if (outputState.hasRequest(OutputState::NoteOn)) {
                    outputState.activeNote = note;
                    outputState.activeBend = outputState.signedBend;
                }
                outputState.clearRequest(OutputState::NoteOn | OutputState::NoteOff);
            }
        }

        // send control change requests
        if (sendCC && outputState.hasRequest(OutputState::ControlChange)) {
            sendMidi(port, MidiMessage::makeControlChange(channel, output.controlNumber(), outputState.control));
            outputState.clearRequest(OutputState::ControlChange);
        }

        // send pitch bend requests (value 0..127 -> 14-bit, modulator center 64 = no bend)
        if (sendCC && outputState.hasRequest(OutputState::PitchBend)) {
            int bend = clamp((int(outputState.control) - 64) * 128, -8192, 8191);
            sendMidi(port, MidiMessage::makePitchBend(channel, bend));
            outputState.clearRequest(OutputState::PitchBend);
        }

        // send channel pressure (aftertouch) requests
        if (sendCC && outputState.hasRequest(OutputState::ChannelPressure)) {
            sendMidi(port, MidiMessage::makeChannelPressure(channel, outputState.control));
            outputState.clearRequest(OutputState::ChannelPressure);
        }
    }
}

void MidiOutputEngine::sendGate(int trackIndex, bool gate) {
    for (int outputIndex = 0; outputIndex < CONFIG_MIDI_OUTPUT_COUNT; ++outputIndex) {
        const auto &output = _midiOutput.output(outputIndex);
        auto &outputState = _outputStates[outputIndex];

        if (output.takesGateFromTrack(trackIndex)) {
            outputState.setRequest(gate ? OutputState::NoteOn : OutputState::NoteOff);
        }
    }
}

void MidiOutputEngine::sendSlide(int trackIndex, bool slide) {
    for (int outputIndex = 0; outputIndex < CONFIG_MIDI_OUTPUT_COUNT; ++outputIndex) {
        const auto &output = _midiOutput.output(outputIndex);
        auto &outputState = _outputStates[outputIndex];

        if (output.takesNoteFromTrack(trackIndex)) {
            if (slide != outputState.slide) {
                outputState.slide = slide;
                outputState.setRequest(OutputState::Slide);
            }
        }
    }
}

void MidiOutputEngine::sendCv(int trackIndex, float cv) {
    for (int outputIndex = 0; outputIndex < CONFIG_MIDI_OUTPUT_COUNT; ++outputIndex) {
        const auto &output = _midiOutput.output(outputIndex);
        auto &outputState = _outputStates[outputIndex];

        if (output.takesNoteFromTrack(trackIndex)) {
            if (output.event() == MidiOutput::Output::Event::Note && output.microtune()) {
                int note, bend;
                cvToNoteBend(cv, output.bendRange(), note, bend);
                outputState.note = note;
                outputState.signedBend = bend;
            } else {
                outputState.note = clamp(60 + int(std::floor(cv * 12.f + 0.01f)), 0, 127);
            }
        }

        if (output.takesVelocityFromTrack(trackIndex)) {
            outputState.velocity = clamp(int(std::floor(cv * (127.f / 5.f))), 0, 127);
        }

        if (output.takesControlFromTrack(trackIndex)) {
            int8_t value = clamp(int(std::floor(cv * (127.f / 5.f))), 0, 127);
            if (value != outputState.control) {
                outputState.control = value;
                outputState.setRequest(OutputState::ControlChange);
            }
        }
    }
}

void MidiOutputEngine::sendModulator(int modulatorIndex, int value) {
    int clamped = clamp(value, 0, 127);
    for (int outputIndex = 0; outputIndex < CONFIG_MIDI_OUTPUT_COUNT; ++outputIndex) {
        const auto &output = _midiOutput.output(outputIndex);
        auto &outputState = _outputStates[outputIndex];
        if (output.takesContinuousFromModulator(modulatorIndex)) {
            if (clamped != outputState.control) {
                outputState.control = clamped;
                switch (output.event()) {
                case MidiOutput::Output::Event::PitchBend:
                    outputState.setRequest(OutputState::PitchBend);
                    break;
                case MidiOutput::Output::Event::ChannelPressure:
                    outputState.setRequest(OutputState::ChannelPressure);
                    break;
                default:
                    outputState.setRequest(OutputState::ControlChange);
                    break;
                }
            }
        }
    }
}

void MidiOutputEngine::sendProgramChange(int channel, int programNumber) {
    auto pgmChangeMessage = MidiMessage::makeProgramChange(channel, programNumber);

    sendMidi(MidiPort::Midi, pgmChangeMessage);
    sendMidi(MidiPort::UsbMidi, pgmChangeMessage);
}

void MidiOutputEngine::sendMalekkoSelectHandshake(int channel) {
    sendMidi(MidiPort::Midi, MidiMessage::makeControlChange(channel, 20, 127));
    sendMidi(MidiPort::Midi, MidiMessage::makeControlChange(channel, 16, 64));
}

void MidiOutputEngine::sendMalekkoSelectReleaseHandshake(int channel) {
    sendMidi(MidiPort::Midi, MidiMessage::makeControlChange(channel, 20, 0));
}

void MidiOutputEngine::sendMalekkoSaveHandshake(int channel) {
    sendMidi(MidiPort::Midi, MidiMessage::makeControlChange(channel, 16, 127));
}

void MidiOutputEngine::resetOutput(int outputIndex) {
    auto &outputState = _outputStates[outputIndex];

    MidiPort port = MidiPort(outputState.target.port());
    int channel = outputState.target.channel();

    if (outputState.activeNote >= 0) {
        sendMidi(port, MidiMessage::makeNoteOff(channel, outputState.activeNote));
    }

    if (outputState.event == MidiOutput::Output::Event::Note) {
        // portamento off
        sendMidi(port, MidiMessage::makeControlChange(channel, 65, 0));
        // all sound off
        sendMidi(port, MidiMessage::makeControlChange(channel, 120, 0));
    }

    outputState.reset();
}

void MidiOutputEngine::sendMidi(MidiPort port, const MidiMessage &message) {
    // MidiMessage::dump(message);
    // always use cable 0
    _engine.sendMidi(port, 0, message);
}
