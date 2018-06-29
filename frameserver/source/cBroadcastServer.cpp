/*
 * Distributed under the OSI-approved Apache License, Version 2.0.  See
 * accompanying file Copyright.txt for details.
 */

#include <sstream>
#include <time.h>

#include <cBroadcastServer.h>
#include <cMouseEventHandler.h>
#include <cKeyboardHandler.h>
#include <cMessageHandler.h>

#include "cPNGEncoder.h"

#ifdef JPEG_ENCODING
#include "cTurboJpegEncoder.h"
#endif




unsigned char webSocketKey;
//extern bool		keyChangedFlag;

broadcast_server::broadcast_server() {
	// Set logging settings
	m_server.set_error_channels(websocketpp::log::elevel::all);
	m_server.set_access_channels(
			websocketpp::log::alevel::all
					^ websocketpp::log::alevel::frame_payload);

	m_server.init_asio();

	// set up access channels to only log interesting things
	m_server.clear_access_channels(websocketpp::log::alevel::all);
	//m_server.set_access_channels(websocketpp::log::alevel::connect);
	//m_server.set_access_channels(websocketpp::log::alevel::disconnect);
	//m_server.set_access_channels(websocketpp::log::alevel::app);

	m_server.set_open_handler(bind(&broadcast_server::on_open, this, ::_1));
	m_server.set_close_handler(bind(&broadcast_server::on_close, this, ::_1));
	m_server.set_message_handler(
			bind(&broadcast_server::on_message, this, ::_1, ::_2));

	needMoreFrames 	= false;
	saveFrame		= false;
	stop 			= true;

	mouseHandler = 0;
	keyboardHandler = 0;
	messageHandler = 0;



#ifdef	JPEG_ENCODING
	targetTime = TIME_RESPONSE;
	jpegQuality = JPEG_QUALITY;
	stTimer1 = high_resolution_clock::now();
	stTimer2 = stTimer1;
	jpegEncoder = new cTurboJpegEncoder();
	jpegEncoder->setEncoderParams(jpegQuality);
	jpegEncoder->setImageParams(IMAGE_WIDTH*RESOLUTION_FACTOR, IMAGE_HEIGHT*RESOLUTION_FACTOR);
	if (!(jpegEncoder->initEncoder())) {
		cout << "Warning: JPEG Encoder failed at initialization \n";
	}
	cout << "\nJPEG Encoder initialized\n";
#endif

	pngEncoder = new cPNGEncoder ();
	pngEncoder->setImageParams(IMAGE_WIDTH, IMAGE_HEIGHT);
	if (!(pngEncoder->initEncoder()))
	{
		std::cout << "Warning: Websocket's PNG Encoder failed at initialization \n";
	}
	std::cout << "\nWebsocket's PNG Encoder initialized\n";
}
//
//=======================================================================================
//
broadcast_server::~broadcast_server() {
	// TODO: stop the server
#ifdef JPEG_ENCODING
	delete jpegEncoder;
	jpegEncoder = 0;
#endif
}

void broadcast_server::on_open(connection_hdl hdl) {
	m_connections.insert(hdl);
}
//
//=======================================================================================
//
void broadcast_server::on_close(connection_hdl hdl)
{
	std::cout << "Web browser closed\n";
	m_connections.erase(hdl);
}
//
//=======================================================================================
//
void broadcast_server::on_message(connection_hdl hdl, server::message_ptr msg) {
	// TODO: Process Interaction msgs
	std::stringstream val;
	int type;
	//int buttonMask;
	//int xPosition;
	//int yPosition;

	for (auto it : m_connections) {
		try {
			val << msg->get_payload();

			if (!stop) {
				type = val.str().data()[0];
				parse(type, &val);
			}
			if (val.str().compare("NXTFR") == 0
					|| val.str().compare("STVIS") == 0) {
#ifdef	JPEG_ENCODING
				stTimer2 = high_resolution_clock::now();
//				adjustJpegQuality();
#endif
				needMoreFrames = true;
				stop = false;
				//std::cout << "NXTFR" << std::endl;
			}
			if (val.str().compare("SAVE ") == 0)
			{
				saveFrame = true;
			}
			if (val.str().compare("END  ") == 0)
			{
				stop = true;
				std::cout << "dedos";
				needMoreFrames = false;
			}
		} catch (const websocketpp::lib::error_code& e) {
			std::cout << "SEND failed because: " << e << "(" << e.message()
					<< ")" << std::endl;
		}
	}

}
//
//=======================================================================================
//
void broadcast_server::stop_listening( )
{
	try
	{
		m_server.stop_listening( );
	} catch (const std::exception & e) {
		std::cout << e.what() << std::endl;
	} catch (websocketpp::lib::error_code e) {
		std::cout << e.message() << std::endl;
	} catch (...) {
		std::cout << "other exception" << std::endl;
	}
	for (auto& it : m_connections) {
		try
		{
			m_server.close(it, websocketpp::close::status::normal, "");
		}
		catch(websocketpp::lib::error_code ec)
		{
			std::cout<<"lib::error_code "<<ec<<std::endl;
		}
	}
}
//
//=======================================================================================
//
void broadcast_server::parse(int type, std::stringstream *value) {
	switch (type) {
	case MOUSE_EVENT:
		if (mouseHandler) {
			mouseHandler->parse(value);
		} else {
			std::cout << "Is Mouse handler initialized? \n";
		}
		break;
	case KEY_EVENT:
		if (keyboardHandler) {
			keyboardHandler->parse(value);
		} else {
			std::cout << "Is keyboard handler initialized? \n";
		}
		break;
	case MESSAGE_EVENT:
		if (messageHandler) {
			messageHandler->parse(value);
			//needMoreFrames	= messageHandler->getFrameFlag 	(	);
			//stop			= messageHandler->stopStreaming (	);
		} else {
			std::cout << "Is Message handler initialized? \n";
		}
		break;

	}
}
//
//=======================================================================================
//
void broadcast_server::run(uint16_t port) {
	m_server.listen(port);
	m_server.start_accept();
	// Start the ASIO io_service run loop
	try {
		m_server.run();
	} catch (const std::exception & e) {
		std::cout << e.what() << std::endl;
	} catch (websocketpp::lib::error_code e) {
		std::cout << e.message() << std::endl;
	} catch (...) {
		std::cout << "other exception" << std::endl;
	}
}
//
//=======================================================================================
//
void broadcast_server::sendFrame(float *img) {
	//setFrame (img);
	con_list::iterator it;
	for (it = m_connections.begin(); it != m_connections.end(); it++) {
		try {
			// when img is unsigned char
			//m_server.send(it, m_img, (size_t)width*height*3 , websocketpp::frame::opcode::BINARY);
			// when img is float
			m_server.send(*it, img,
					(size_t) IMAGE_WIDTH * IMAGE_HEIGHT * 3 * sizeof(float),
					websocketpp::frame::opcode::BINARY);

			needMoreFrames = false;
		} catch (const websocketpp::lib::error_code& e) {
			std::cout << "SEND failed because: " << e << "(" << e.message()
					<< ")" << std::endl;
		}
	}
}
//
//=======================================================================================
//
void broadcast_server::sendFrame(unsigned char *img) {

#ifdef CHANGE_RESOLUTION
	static unsigned int half_w = IMAGE_WIDTH*RESOLUTION_FACTOR;
	static unsigned int half_h = IMAGE_HEIGHT*RESOLUTION_FACTOR;
	unsigned char *halfImg = new unsigned char [half_w * half_h * 3 ];
	scale ( img, halfImg, RESOLUTION_FACTOR );
#endif

	if (saveFrame)
	{
		char date[128];
		time_t rawtime;
		struct tm * timeinfo;
		time(&rawtime);
		timeinfo = localtime (&rawtime);
		strftime (date,sizeof(date),"%Y-%m-%d_%OH_%OM_%OS",timeinfo);
		std::stringstream filename;
		filename << "Sight_" << date << ".png";
		pngEncoder->savePNG(filename.str(), img);
		std::cout << filename.str().data() << " saved!\n";
		saveFrame = false;
	}


#ifdef JPEG_ENCODING

#ifdef TIME_METRICS
	high_resolution_clock::time_point t1 = high_resolution_clock::now();
#endif

#ifdef CHANGE_RESOLUTION
	if (!jpegEncoder->encode(halfImg))
#else
	if (!jpegEncoder->encode(img))
#endif
	{
			cout << "Encoding error \n";
	}

#ifdef TIME_METRICS
	high_resolution_clock::time_point t2 = high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::microseconds>( t2 - t1 ).count();
	std::cout << "JPEG compression: " << duration/1000 << std::endl;
#endif
	stTimer1 = high_resolution_clock::now();
	for (auto it : m_connections) {
		try {

			m_server.send(it, jpegEncoder->compressedImg,
					(size_t) jpegEncoder->getJpegSize(),
					websocketpp::frame::opcode::BINARY);

			/*
			m_server.send(it, std::string (reinterpret_cast<char*>(jpegEncoder->compressedImg)),
					(size_t) jpegEncoder->getJpegSize(),
					websocketpp::frame::opcode::BINARY);
					*/
			needMoreFrames = false;
		} catch (const websocketpp::lib::error_code& e) {
			std::cout << "SEND failed because: " << e << "(" << e.message()
					<< ")" << std::endl;
		}
	}
#else
	con_list::iterator it;
	for (it = m_connections.begin(); it != m_connections.end(); it++)
	{
		try
		{
#ifdef CHANGE_RESOLUTION
			m_server.send(*it, halfImg, (size_t)((IMAGE_WIDTH/2)*(IMAGE_HEIGHT/2)*3) , websocketpp::frame::opcode::BINARY);
#else
			// when img is unsigned char
			m_server.send(*it, img, (size_t)IMAGE_WIDTH*IMAGE_HEIGHT*3 , websocketpp::frame::opcode::BINARY);
			//m_server.send(it, "END  ", websocketpp::frame::opcode::text);
#endif
			needMoreFrames = false;
		}
		catch (const websocketpp::lib::error_code& e)
		{
			std::cout << "SEND failed because: " << e
			<< "(" << e.message() << ")" << std::endl;
		}
	}
#endif

#ifdef CHANGE_RESOLUTION
	delete [] halfImg;
#endif
}
//
//=======================================================================================
//
void broadcast_server::setMouseHandler(cMouseHandler *mouseH) {
	mouseHandler = mouseH;
}
//
//=======================================================================================
//
void broadcast_server::setKeyboardHandler(cKeyboardHandler *keyboardH) {
	keyboardHandler = keyboardH;
}
//
//=======================================================================================
//
void broadcast_server::setMessageHandler(cMessageHandler *messageH) {
	messageHandler = messageH;
}
//
//=======================================================================================
//
#ifdef JPEG_ENCODING
void broadcast_server::adjustJpegQuality() {
	stDuration = std::chrono::duration_cast < std::chrono::microseconds
			> (stTimer2 - stTimer1);
	//std::cout << "Image Size: " << jpegEncoder->getJpegSize()/1000.0f << " (KB) Image transport duration: " << stDuration.count()/1000.0f << " (ms) \n";
	//std::cout << "Rate: " << (jpegEncoder->getJpegSize()/1000.0f) / (stDuration.count()/1000000.0f) << " (KBps) \n";

	if ((stDuration.count() / 1000.0f) > targetTime) {
		jpegQuality -= 1;
		if (jpegQuality < 30)
			jpegQuality = 30;
		jpegEncoder->setEncoderParams(jpegQuality);
//		std::cout << std::endl << "jpegQuality: " << jpegQuality << std::endl;
	} else {
		jpegQuality += 1;
		if (jpegQuality > 100)
			jpegQuality = 100;
		jpegEncoder->setEncoderParams(jpegQuality);
//		std::cout << std::endl << "jpegQuality: " << jpegQuality << std::endl;
	}
}
#endif
//
//=======================================================================================
//
#ifdef CHANGE_RESOLUTION
void broadcast_server::scale ( unsigned char *in, unsigned char *out, float factor )
{
	unsigned int half_w = IMAGE_WIDTH*factor;
	unsigned int half_h = IMAGE_HEIGHT*factor;

/*
#pragma omp parallel sections num_threads(2) // starts a new team
	{
#pragma omp section
		for (unsigned int j=0; j<half_h ; j++)
		{
			for (unsigned int i=0;i< half_w/2; i++)
			{
				unsigned int halfIdx	= (half_w * 3) * j + i*3;
				unsigned int idx		= ( j / factor ) * (IMAGE_WIDTH *3) + (i / factor ) * 3;

				out[halfIdx  ] = in[idx  ];
				out[halfIdx+1] = in[idx+1];
				out[halfIdx+2] = in[idx+2];
			}
		}

#pragma omp section
		for (unsigned int j=0; j<half_h ; j++)
		{
			for (unsigned int i=half_h/2;i< half_w; i++)
			{
				unsigned int halfIdx	= (half_w * 3) * j + i*3;
				unsigned int idx		= ( j / factor ) * (IMAGE_WIDTH *3) + (i / factor ) * 3;

				out[halfIdx  ] = in[idx  ];
				out[halfIdx+1] = in[idx+1];
				out[halfIdx+2] = in[idx+2];
			}
		}
	}
	*/
/*
#pragma omp parallel for num_threads (2)
	for (unsigned int i=0; i<half_w * half_h; i++)
	{
		unsigned int x = i%half_w;
		unsigned int y = i/half_w;

		unsigned int halfIdx	= (half_w * 3) * y + x*3;
		unsigned int idx 		= ( y / factor ) * (IMAGE_WIDTH *3) + (x / factor ) * 3;

		out[halfIdx  ] = in[idx  ];
		out[halfIdx+1] = in[idx+1];
		out[halfIdx+2] = in[idx+2];

	}
*/

//#pragma omp parallel for collapse (2)
	for (unsigned int j=0; j<half_h ; j++)
	{
		for (unsigned int i=0;i< half_w; i++)
		{
			unsigned int halfIdx	= (half_w * 3) * j + i*3;
            unsigned int idx		= ( j / factor ) * (IMAGE_WIDTH *3) + (i / factor ) * 3;

            out[halfIdx  ] = in[idx  ];
			out[halfIdx+1] = in[idx+1];
			out[halfIdx+2] = in[idx+2];
		}
	}

}
#endif
//
//=======================================================================================
//
