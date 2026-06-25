// ParameterModel — the declarative parameter table.
//
// One ParamSpec per editable MS2000 parameter ties together: the UI control to
// draw, the SysEx byte/bitfield it lives in, and the CC used for real-time
// control. Both the UI and the MidiEngine are generated from this single table,
// so adding a parameter is a one-line change. Pure C++/STL, no JUCE.
#pragma once

#include "Program.h"
#include <string>
#include <vector>

namespace ms2000 {

// Sections mirror the MS2000 front panel (see docs/MS2K_Hardware_Controls.csv):
// no standalone "Pitch" (it lives within the oscillator group); Mod + Delay are
// one "Effects" block; Portamento is its own section.
enum class Section {
    Voice, Osc1, Osc2, Mixer, Filter, Amp,
    Eg1, Eg2, Lfo1, Lfo2, Patch, Portamento,
    Effects, Eq, Arp
};

enum class ValueType { Continuous, Enum, Bool };
enum class Scope     { Program, Timbre };

// Timbre Select is a performance control (Global-assignable "TimbSel Ctrl No"),
// not a program-byte parameter, so it lives here rather than in the table.
// Hardware-validated CC on this unit (0=Timbre 1, 1=Timbre 1&2, 2-127=Timbre 2).
// Used by the timbre tabs in Milestone 4 (send on switch / receive to follow).
constexpr int kTimbreSelectCC = 95;

struct ParamSpec {
    std::string id;
    std::string label;
    Section     section;
    Scope       scope;
    ValueType   type;
    int         offset;        // byte offset (in common, or within timbre block)
    uint8_t     shift;         // bitfield shift
    uint8_t     mask;          // bitfield mask AFTER shift (0xFF = whole byte)
    int         displayMin;
    int         displayMax;
    int         displayOffset; // raw = display + displayOffset
    bool        bipolar;       // UI hint: draw centered
    int         cc;            // assignable CC number, or -1 if none
    std::vector<std::string> enums; // labels for ValueType::Enum
    const int8_t* table = nullptr;  // optional MIDI-value -> display-value map (128 entries)

    int rawMin() const { return displayMin + displayOffset; }
    int rawMax() const { return displayMax + displayOffset; }
};

// The table (built once, returned by reference).
const std::vector<ParamSpec>& parameterTable();

// Look up a spec by id (nullptr if not found).
const ParamSpec* findParam(const std::string& id);

// ---- value access (display units) -----------------------------------------
int  getDisplay(const Program& p, int timbre, const ParamSpec& s);
void setDisplay(Program& p, int timbre, const ParamSpec& s, int displayValue);

// ---- CC value for a given raw value ---------------------------------------
// Continuous/bipolar -> raw (already 0..127). Enum -> band centre. Bool -> 0/127.
int ccValueForRaw(const ParamSpec& s, int rawValue);

// Inverse: decode an incoming CC value (0..127) back to a raw parameter value.
// Continuous -> value; Enum -> band index; Bool -> 0/1.
int rawFromCcValue(const ParamSpec& s, int ccValue);

// Convenience: current raw value of a param in a program.
int getRaw(const Program& p, int timbre, const ParamSpec& s);

} // namespace ms2000
