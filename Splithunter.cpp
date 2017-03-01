#include <getopt.h>
#include "SeqLib/RefGenome.h"
#include "SeqLib/BWAWrapper.h"
#include "SeqLib/BamReader.h"
#include "SeqLib/SeqLibUtils.h"

using namespace SeqLib;
using namespace std;


static const char *USAGE_MESSAGE =
"Program: Splithunter \n"
"Contact: Haibao Tang \n"
"Usage: Splithunter bamfile [options]\n\n"
"Commands:\n"
"  --verbose,   -v        Set verbose output\n"
"  --reference, -r <file> Reference genome if using BWA-MEM realignment\n"
"\nReport bugs to <htang@humanlongevity.com>\n\n";

namespace opt {
    static bool verbose = false;
    static string bam;
    static string reference = "/mnt/ref/hg38.upper.fa";
}

static const char* shortopts = "hvb:r:";
static const struct option longopts[] = {
    { "help",                    no_argument, NULL, 'h' },
    { "verbose",                 no_argument, NULL, 'v' },
    { "reference",               required_argument, NULL, 'r' },
    { NULL, 0, NULL, 0 }
};


// Where work is done
int run() {
    cout << opt::bam << endl;
    cout << opt::reference << endl;

    RefGenome ref;
    ref.LoadIndex(opt::reference);

    // get sequence at given locus
    string seq = ref.QueryRegion("chr14", 22386000, 22477000);
    //cout << seq << endl;

    // Make an in-memory BWA-MEM index of region
    BWAWrapper bwa;
    UnalignedSequenceVector usv = {{"chr_reg1", seq}};
    bwa.ConstructIndex(usv);

    BamReader br;
    br.Open(opt::bam);
    BamRecord r;
    const bool hardclip = false;
    const float secondary_cutoff = .9;
    const int secondary_cap = 0;
    const int PAD = 30;  // threshold for a significant match

    int counts = 0;
    string leftPart, rightPart;

    while (br.GetNextRecord(r)) {
        BamRecordVector results;
        if (r.NumClip() < PAD || r.NumClip() > r.Length() - PAD) continue;
        //cout << "BEFORE: " << r.GetCigar() << endl;
        bwa.AlignSequence(r.Sequence(), r.Qname(),
                          results, hardclip, secondary_cutoff, secondary_cap);

        cout << "FOUND " << results.size() << " ALIGNMENTS" << endl;
        for (auto& i : results)
        {
            if (i.NumClip() < PAD || i.NumClip() > i.Length() - PAD) continue;
            //cout << "AFTER : " << i.GetCigar() << endl;
            cout << i;
            counts++;
            // Bipartite alignment: 0-index coordinate at this point
            int32_t queryStart = i.AlignmentPosition();
            int32_t queryEnd   = i.AlignmentEndPosition();
            int32_t readLength = i.Length();
            cout << "start: " << queryStart << " end: " << queryEnd << " len: " << readLength << endl;
            if (queryStart > PAD) {
                leftPart = i.Sequence().substr(0, queryStart - 1);
                rightPart = i.Sequence().substr(queryStart, readLength);
            } else if (queryEnd < readLength - PAD) {
                leftPart = i.Sequence().substr(0, queryEnd);
                rightPart = i.Sequence().substr(queryEnd + 1, readLength);
            } else continue;
            cout << "leftPart : " << leftPart << endl;
            cout << "rightPart: " << rightPart << endl;

            BamRecordVector resultsL, resultsR;
            bwa.AlignSequence(leftPart, r.Qname() + "L",
                              resultsL, hardclip, secondary_cutoff, secondary_cap);

            bwa.AlignSequence(rightPart, r.Qname() + "R",
                              resultsR, hardclip, secondary_cutoff, secondary_cap);

            if (resultsL.size() > 0 && resultsR.size() > 0) {
                for (auto& l : resultsL) cout << "[L] " << l;
                for (auto& r : resultsR) cout << "[R] " << r;
            }

            // TODO: Make sure the left and right alignment are distinct
        }
    }
    cout << "Number of alignments:" << counts << endl;

    return 0;
}


// Parse the command line options
int main(int argc, char** argv) {
    if (argc <= 1) {
        cerr << USAGE_MESSAGE;
        return 0;
    }

    // Get the first argument as input
    if (argc > 1)
        opt::bam = string(argv[1]);

    bool die = false;
    bool help = false;

    for (char c; (c = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1;) {
        istringstream arg(optarg != NULL ? optarg : "");
        switch (c) {
            case 'v': opt::verbose = true; break;
            case 'r': arg >> opt::reference; break;
            default: die = true;
        }
    }

    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    run();
    cout << displayRuntime(start) << endl;

    if (die || help || opt::reference.empty() || opt::bam.empty()) {
        cerr << "\n" << USAGE_MESSAGE;
        if (die) exit(EXIT_FAILURE);
        else exit(EXIT_SUCCESS);
    }
}
