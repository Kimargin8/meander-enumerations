// Optional GNU stdio adapter. Uncompressed layers and all small output files
// keep their exact previous representation. The state-transfer code is unchanged.
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <stdexcept>
#include <string_view>
#include <vector>
#include <cstdint>
#include <memory>

extern "C" FILE* __real_fopen(const char*,const char*);
namespace {
constexpr std::array<unsigned char,8> magic={255,'L','R','W','R','Z','0','1'};
constexpr size_t block=1<<20;
struct Codec {
    void* lib=dlopen("libzstd.so.1",RTLD_NOW|RTLD_LOCAL);
    size_t (*bound)(size_t);
    size_t (*compress)(void*,size_t,const void*,size_t,int);
    size_t (*decompress)(void*,size_t,const void*,size_t);
    unsigned (*error)(size_t);
    Codec(){
        if(!lib)throw std::runtime_error("system libzstd.so.1 is required for packed layers");
        bound=reinterpret_cast<decltype(bound)>(dlsym(lib,"ZSTD_compressBound"));
        compress=reinterpret_cast<decltype(compress)>(dlsym(lib,"ZSTD_compress"));
        decompress=reinterpret_cast<decltype(decompress)>(dlsym(lib,"ZSTD_decompress"));
        error=reinterpret_cast<decltype(error)>(dlsym(lib,"ZSTD_isError"));
        if(!bound||!compress||!decompress||!error)throw std::runtime_error("incomplete libzstd API");
    }
};
Codec& codec(){static Codec c;return c;}
struct Filter {
    FILE* raw;
    bool writer,framed;
    size_t pos=0,used=0;
    off64_t logical=0;
    std::vector<unsigned char> plain,compressed;
    Filter(FILE* f,bool w):raw(f),writer(w),framed(!w),plain(block),compressed(codec().bound(block)){}
    void write(const void* b,size_t n){
        if(fwrite(b,1,n,raw)!=n)throw std::runtime_error("packed write");
    }
    void emit(){
        if(!used)return;
        if(!framed&&used<block){write(plain.data(),used);used=0;return;}
        if(!framed){write(magic.data(),magic.size());framed=true;}
        size_t len=codec().compress(compressed.data(),compressed.size(),plain.data(),used,1);
        if(codec().error(len))throw std::runtime_error("ZSTD_compress");
        uint32_t numbers[2]={uint32_t(used),uint32_t(len)};
        write(numbers,sizeof(numbers));write(compressed.data(),len);used=0;
    }
    bool fill(){
        uint32_t numbers[2];
        size_t have=fread(numbers,1,sizeof(numbers),raw);
        if(!have&&feof(raw))return false;
        if(have!=sizeof(numbers)||!numbers[0]||numbers[0]>block||!numbers[1]||numbers[1]>compressed.size())
            throw std::runtime_error("invalid packed block");
        if(fread(compressed.data(),1,numbers[1],raw)!=numbers[1])throw std::runtime_error("truncated packed block");
        size_t len=codec().decompress(plain.data(),plain.size(),compressed.data(),numbers[1]);
        if(codec().error(len)||len!=numbers[0])throw std::runtime_error("ZSTD_decompress");
        pos=0;used=len;return true;
    }
};
ssize_t pull(void* context,char* out,size_t n){
    auto& f=*static_cast<Filter*>(context);size_t done=0;
    try{
        while(done<n){
            if(f.pos==f.used&&!f.fill())break;
            size_t here=std::min(n-done,f.used-f.pos);
            std::memcpy(out+done,f.plain.data()+f.pos,here);done+=here;f.pos+=here;
        }
        f.logical+=done;return done;
    }catch(...){errno=EIO;return -1;}
}
ssize_t push(void* context,const char* in,size_t n){
    auto& f=*static_cast<Filter*>(context);size_t done=0;
    try{
        while(done<n){
            size_t here=std::min(n-done,block-f.used);
            std::memcpy(f.plain.data()+f.used,in+done,here);done+=here;f.used+=here;
            if(f.used==block)f.emit();
        }
        f.logical+=done;return done;
    }catch(...){errno=EIO;return -1;}
}
int position(void* context,off64_t* offset,int whence){
    if(whence!=SEEK_CUR||*offset){errno=ESPIPE;return -1;}
    *offset=static_cast<Filter*>(context)->logical;return 0;
}
int close_filter(void* context){
    std::unique_ptr<Filter> f(static_cast<Filter*>(context));
    bool failed=false;try{if(f->writer)f->emit();}catch(...){failed=true;}
    if(fclose(f->raw)||failed){errno=EIO;return -1;}return 0;
}
}
extern "C" FILE* __wrap_fopen(const char* filename,const char* mode){
    FILE* raw=__real_fopen(filename,mode);
    std::string_view name(filename);
    if(!raw||name.size()<4||name.substr(name.size()-4)!=".bin")return raw;
    bool writer=std::strcmp(mode,"wb")==0;
    if(!writer){
        if(std::strcmp(mode,"rb"))return raw;
        std::array<unsigned char,magic.size()> head{};
        if(fread(head.data(),1,head.size(),raw)!=head.size()||head!=magic){
            rewind(raw);return raw;
        }
    }
    try{
        auto f=std::make_unique<Filter>(raw,writer);
        cookie_io_functions_t callbacks{writer?nullptr:pull,writer?push:nullptr,position,close_filter};
        FILE* wrapped=fopencookie(f.get(),writer?"w":"r",callbacks);
        if(!wrapped){fclose(raw);return nullptr;}
        f.release();return wrapped;
    }catch(...){fclose(raw);errno=EIO;return nullptr;}
}
