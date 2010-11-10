
/* Copyright (c) 2006-2010, Stefan Eilemann <eile@equalizergraphics.com>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 2.1 as published
 * by the Free Software Foundation.
 *  
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef EQ_FRAMEDATA_H
#define EQ_FRAMEDATA_H

#include <eq/client/frame.h>         // enum Frame::Buffer
#include <eq/client/types.h>         // member

#include <eq/fabric/pixelViewport.h> // member
#include <eq/fabric/pixel.h>         // member
#include <eq/fabric/range.h>         // member
#include <eq/fabric/subPixel.h>      // member
#include <eq/net/object.h>           // base class
#include <eq/base/monitor.h>         // member

#include <set>                       // member

namespace eq
{

namespace server
{
    class FrameData;
}
    class  Image;
    class  ROIFinder;

    /**
     * A holder for multiple images.
     *
     * The FrameData is used to connect the Image data for multiple frames.
     * Equalizer uses the same frame data for all input and output frames of the
     * same name. This enables frame-specific parameters to be set on the Frame,
     * and generic parameters (of the output frame) to be set on the FrameData,
     * as well as ready synchronization of the pixel data.
     */
    class FrameData : public net::Object
    {
    public:
        /** Construct a new frame data holder. @version 1.0 */
        EQ_EXPORT FrameData();

        /** Destruct this frame data. @version 1.0 */
        EQ_EXPORT virtual ~FrameData();

        /** @name Data Access */
        //@{
        /** @return the enabled frame buffer attachments. @version 1.0 */
        uint32_t getBuffers() const { return _data.buffers; }

        /**
         * Set the enabled frame buffer attachments.
         *
         * The default buffers are set for Equalizer input and output frames
         * according to the configuration, or to 0 for application-created frame
         * datas.
         */
        void setBuffers( const uint32_t buffers ){ _data.buffers = buffers;}

        /**
         * Get the range.
         *
         * The range is set for Equalizer frames to the range of the frame data
         * relative to the destination channel.
         *
         * @return the database-range.
         * @version 1.0
         */
        const Range& getRange() const { return _data.range; }

        /** Set the range of this frame. @version 1.0 */
        void setRange( const Range& range ) { _data.range = range; }
        
        /** @return the pixel decomposition wrt the destination channel. */
        const Pixel& getPixel() const { return _data.pixel; }
        
        /** @return the subpixel decomposition wrt the destination channel. */
        const SubPixel& getSubPixel() const { return _data.subpixel; }

        /** @return the DPlex period relative to the destination channel. */
        uint32_t getPeriod() const { return _data.period; }

        /** @return the DPlex phase relative to the destination channel. */
        uint32_t getPhase() const { return _data.phase; }

        /** The images of this frame data holder */
        const Images& getImages() const { return _images; }

        /** The covered area. */
        void setPixelViewport( const PixelViewport& pvp ) { _data.pvp = pvp; }
        
        /** Enable/disable alpha usage for newly allocated images. */
        void setAlphaUsage( const bool useAlpha ) { _useAlpha = useAlpha; }

        /** Set the minimum quality after compression. */
        void setQuality( const Frame::Buffer buffer, const float quality );

        /** Set additional zoom for input frames, distributed. @internal */
        void setZoom( const Zoom& zoom ) { _data.zoom = zoom; }

        /** @return the additional zoom. @internal */
        const Zoom& getZoom() const { return _data.zoom; }
        //@}

        /**
         * @name Operations
         */
        //@{
        /** 
         * Allocate and add a new image.
         * 
         * @return the image.
         */
        EQ_EXPORT Image* newImage( const Frame::Type type,
                                   const DrawableConfig& config );

        /** Flush the frame by deleting all images. */
        void flush();

        /** Clear the frame by recycling the attached images. */
        EQ_EXPORT void clear();

        /** 
         * Read back a set of images according to the current frame data.
         * 
         * The newly read images are added to the data, existing images are
         * retained.
         *
         * @param frame the corresponding output frame holder.
         * @param glObjects the GL object manager for the current GL context.
         * @param config the configuration of the source frame buffer.
         */
        void readback( const Frame& frame, 
                       util::ObjectManager< const void* >* glObjects,
                       const DrawableConfig& config );

        /** 
         * Transmit the frame data to the specified node.
         *
         * Used internally after readback to push the image data to the input
         * frame nodes. Do not use directly.
         * 
         * @param toNode the receiving node.
         * @param frameNumber the current frame number
         * @param channel the sender channel for statistics
         * @param taskID per-channel task counter
         * @param statisticsIndex the index where statistique will be added
         */        
        void transmit( net::NodePtr toNode, const uint32_t frameNumber,
                       Channel* channel, const uint32_t taskID, 
                       const uint32_t statisticsIndex );

        /** 
         * Set the frame data ready.
         * 
         * The frame data is automatically set ready by syncReadback
         * and upon receiving of the transmit commands.
         */
        void setReady();

        /** @return true if the frame data is ready, false if not. */
        bool isReady() const   { return _readyVersion >= getVersion(); }

        /** Wait for the frame data to become available. */
        void waitReady() const { _readyVersion.waitGE( getVersion( )); }

        /** 
         * Add a listener which will be incremented when the frame is
         * ready.
         * 
         * @param listener the listener.
         */
        void addListener( base::Monitor<uint32_t>& listener );

        /** 
         * Remove a frame listener.
         * 
         * @param listener the listener.
         */
        void removeListener( base::Monitor<uint32_t>& listener );
        
        /** 
         * Disable the usage of a frame buffer attachment for all images.
         * 
         * @param buffer the buffer to disable.
         */
        void disableBuffer( const Frame::Buffer buffer )
            { _data.buffers &= ~buffer; }
 
        /** @internal */
        void useSendToken( const bool use ) { _useSendToken = use; }
        //@}

        /** @warning internal use only. */
        void update( const uint32_t version );

    protected:
        virtual ChangeType getChangeType() const { return INSTANCE; }
        virtual void getInstanceData( net::DataOStream& os );
        virtual void applyInstanceData( net::DataIStream& is );

        /** @sa net::Object::attachToSession */
        virtual void attachToSession( const uint32_t id, 
                                      const uint32_t instanceID, 
                                      net::Session*  session );

    private:
        struct Data
        {
            Data() : frameType( Frame::TYPE_MEMORY ), buffers( 0 ), period( 1 )
                   , phase( 0 ) {}

            PixelViewport pvp;
            Frame::Type   frameType;
            uint32_t      buffers;
            uint32_t      period;
            uint32_t      phase;
            Range         range;     //<! database-range of src wrt to dest
            Pixel         pixel;     //<! pixel decomposition of source
            SubPixel      subpixel;  //<! subpixel decomposition of source
            Zoom          zoom;
        } _data;

        friend class server::FrameData;
        friend struct FrameDataReadyPacket;

        Images _images;
        Images _imageCache;
        base::Lock _imageCacheLock;

        ROIFinder* _roiFinder;

        struct ImageVersion
        {
            ImageVersion( Image* _image, const uint32_t _version )
                    : image( _image ), version( _version ) {}

            Image*   image;
            uint32_t version;
        };
        std::list<ImageVersion> _pendingImages;

        typedef std::deque< net::Command* > Commands;
        Commands _readyVersions;

        typedef base::Monitor< uint32_t > Monitor;
        /** Data ready monitor synchronization primitive. */
        Monitor _readyVersion;

        typedef std::vector< Monitor* > Monitors;
        /** External monitors for readiness synchronization. */
        base::Lockable< Monitors, base::SpinLock > _listeners;

        bool _useAlpha;
        bool _useSendToken;
        float _colorQuality;
        float _depthQuality;

        union // placeholder for binary-compatible changes
        {
            char dummy[32];
        };

        /** Allocate or reuse an image. */
        Image* _allocImage( const eq::Frame::Type type,
                            const DrawableConfig& config );

        /** Apply all received images of the given version. */
        void _applyVersion( const uint32_t version );

        /** Set a specific version ready. */
        void _setReady( const uint32_t version );

        /* The command handlers. */
        bool _cmdTransmit( net::Command& command );
        bool _cmdReady( net::Command& command );
        bool _cmdUpdate( net::Command& command );

        EQ_TS_VAR( _commandThread );
    };
    std::ostream& operator << ( std::ostream& os, const FrameData* data );
}

#endif // EQ_FRAMEDATA_H

