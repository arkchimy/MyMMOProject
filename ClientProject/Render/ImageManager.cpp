#include "ImageManager.h"
#include "image.h"
#include "../utility/Common.h"

namespace render
{
    ImageManager::~ImageManager()
    {
        for (auto element : mCache)
        {
            delete element.second;
        }
    }
    const Image* ImageManager::Load(const std::string& filename)
    {
        Image* retval;
        auto Cacheiter = mCache.find(filename);

        if (Cacheiter == mCache.end())
        {
            retval = new Image();
            retval->ReadFromFile(filename.c_str());
            mCache.insert({ filename ,retval });

            auto Refiter = mRefcnt.find(filename);
            if (Refiter != mRefcnt.end())
            {
                // ?덉쑝硫?留먮룄 ?덈릺???곹솴.
                RT_ASSERT(false);
            }
            mRefcnt.insert({ filename,0 });
        }
        else
        {
            retval = Cacheiter->second;
        }
        auto Refiter = mRefcnt.find(filename);
        if (Refiter == mRefcnt.end())
        {
            // ?놁쑝硫?留먮룄 ?덈릺???곹솴.
            RT_ASSERT(false);
        }
        ++Refiter->second;
        return retval;
    }

    void ImageManager::Release(const std::string& filename)
    {
        auto iter = mRefcnt.find(filename);
        if (iter == mRefcnt.end())
        {
            RT_ASSERT(false);
        }
        int refcnt = --iter->second;
        if (refcnt == 0)
        {
            mRefcnt.erase(iter);
            auto cacheIter = mCache.find(filename);
            if (cacheIter == mCache.end())
            {
                RT_ASSERT(false);
            }
            Image* img = cacheIter->second;
            mCache.erase(cacheIter);
            delete img;
        }
    }

};