#include <cstdint>
#include <vector>
#include <queue>
#include <tuple>

struct event_t
{
    uint32_t t;
    uint8_t x, y;
    int8_t delta;
    uint8_t abs;
};


std::vector<event_t> build_events(
    unsigned t, unsigned w, unsigned h, unsigned max_changes,
    unsigned &unq,
    const std::vector<uint8_t> &current,
    std::vector<uint8_t> &working
)
{
    std::priority_queue<std::tuple<unsigned,unsigned,unsigned>> changes;

    for(unsigned y=0; y<h; y++){
        for(unsigned x=0; x<w; x++){
            int curr=current[y*w+x];
            int prev=working[y*w+x];
            if(curr!=prev){
                int delta=std::abs(prev-curr);
                changes.push({delta,unq, y*w+x});
            }
            unq++;
            if(unq==19937){
                unq=0;
            }
        }
    }

    unsigned todo=std::min((size_t)max_changes, changes.size());
    std::vector<event_t> res;
    res.reserve(todo);
    for(unsigned i=0; i<todo; i++){
        const auto &e = changes.top();
        int pos=std::get<2>(e);
        res.push_back({t, pos/w, pos%w, current[pos]-working[pos], current[pos]});
        changes.pop();
    }

    return res;
}


int main(int argc, char *argv[])
{
    int w=atoi(argv[1]);
    int h=atoi(argv[2]);
    int max_changes=atoi(argv[3]);

    std::vector<uint8_t> current(w*h,0);
    std::vector<uint8_t> working(w*h,127);

    unsigned unq;
    unsigned t=0;
    while(1){
        if(w*h!=fread(&current[0], 1, w*h, stdin)){
            if(feof(stdin)){
                return 0;
            }else{
                return 1;
            }
        }

        auto changes=build_events(t, w, h, max_changes, unq, current,working);
        static_assert(sizeof(event_t)==8, "Struct size mis-match");

        uint32_t count=changes.size();
        if(4!=fwrite(&count, 4, 1, stdout)){
            return 1;
        }
        if(changes.size()!=fwrite(&changes[0], sizeof(event_t), changes.size(), stdout)){
            return 1;
        }

        t++;
    }
}