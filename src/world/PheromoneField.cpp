#include "PheromoneField.h"

PheromoneField g_pheromoneField;

void PheromoneField::saveTo(std::ofstream& f) const {
    f << "PHERO_V1 " << cols_ << " " << rows_ << " " << cell_ << "\n";
    if (!ready_) { f << "0\n"; return; }
    f << "1\n";
    // Sparse: only non-zero cells, per channel.
    for (int ch = 0; ch < CHANNELS; ++ch) {
        int nz = 0;
        for (float v : grid_[ch]) if (v > 0.0f) nz++;
        f << nz << "\n";
        for (size_t i = 0; i < grid_[ch].size(); ++i)
            if (grid_[ch][i] > 0.0f) f << i << " " << grid_[ch][i] << "\n";
    }
}

void PheromoneField::loadFrom(std::ifstream& f) {
    std::string marker;
    if (!(f >> marker) || marker != "PHERO_V1") return;
    int c, r; float cs;
    f >> c >> r >> cs;
    int hasData; f >> hasData;
    cols_ = c; rows_ = r; cell_ = cs;
    for (auto& g : grid_) g.assign((size_t)cols_ * rows_, 0.0f);
    ready_ = true;
    if (!hasData) return;
    for (int ch = 0; ch < CHANNELS; ++ch) {
        int nz; f >> nz;
        for (int i = 0; i < nz; ++i) {
            size_t idx; float v;
            f >> idx >> v;
            if (idx < grid_[ch].size()) grid_[ch][idx] = v;
        }
    }
}
