/**
 * BUILD ART file editing tool
 * @author Jonathon Fowler
 * @license Artistic License 2.0 (http://www.perlfoundation.org/artistic_license_2_0)
 */

#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>

using namespace std;

void usage()
{
	cout << "BUILD ART file editing tool" << endl;
	cout << "by Jonathon Fowler <jf@jonof.id.au>" << endl;
	cout << endl;
	cout << "  arttool create [options]" << endl;
	cout << "    -f <filenum>   Selects which numbered ART file to create (default 0)" << endl;
	cout << "    -o <offset>    Specifies the first tile in the file (default 0)" << endl;
	cout << "    -n <ntiles>    The number of tiles for the art file (default 256)" << endl;
	cout << "    Creates an empty ART file named 'tilesXXX.art'" << endl;
	cout << endl;
	cout << "  arttool addtile [options] <tilenum> <filename>" << endl;
	cout << "    -x <pixels>    X-centre" << endl;
	cout << "    -y <pixels>    Y-centre" << endl;
	cout << "    -ann <frames>  Animation frame span" << endl;
	cout << "    -ant <type>    Animation type (0=none, 1=oscillate, 2=forward, 3=reverse)" << endl;
	cout << "    -ans <speed>   Animation speed" << endl;
	cout << "    Adds a tile to the 'tilesXXX.art' set from a TGA or PCX source" << endl;
	cout << endl;
	cout << "  arttool rmtile <tilenum>" << endl;
	cout << "    Removes a tile from the 'tilesXXX.art' set" << endl;
	cout << endl;
	cout << "  arttool tileprop [options] <tilenum>" << endl;
	cout << "    -x <pixels>    X-centre" << endl;
	cout << "    -y <pixels>    Y-centre" << endl;
	cout << "    -ann <frames>  Animation frame span, may be -ve" << endl;
	cout << "    -ant <type>    Animation type (0=none, 1=oscillate, 2=forward, 3=reverse)" << endl;
	cout << "    -ans <speed>   Animation speed" << endl;
	cout << "    Changes tile properties" << endl;
	cout << endl;
}

class ARTFile {
private:
	string filename_;
	long localtilestart_;
	long localtileend_;
	short * tilesizx_;
	short * tilesizy_;
	long * picanm_;
	
	// for removing or replacing tile data
	int markprelength_, markskiplength_, markpostlength_;
	char * insert_;
	int insertlen_;
	
	void writeShort(ofstream &ofs, short s)
	{
		char d[2] = { s&255, (s>>8)&255 };
		ofs.write(d, 2);
	}
	
	void writeLong(ofstream &ofs, long l)
	{
		char d[4] = { l&255, (l>>8)&255, (l>>16)&255, (l>>24)&255 };
		ofs.write(d, 4);
	}
	
	short readShort(ifstream &ifs)
	{
		char d[2];
		unsigned short s;
		ifs.read(d, 2);
		s = (unsigned short)d[0];
		s |= (unsigned short)d[1] << 8;
		return (short)s;
	}
	
	long readLong(ifstream &ifs)
	{
		char d[4];
		unsigned long l;
		ifs.read(d, 4);
		l = (unsigned long)d[0];
		l |= (unsigned long)d[1] << 8;
		l |= (unsigned long)d[2] << 16;
		l |= (unsigned long)d[3] << 24;
		return (long)l;
	}
	
	void dispose()
	{
		if (tilesizx_) delete [] tilesizx_;
		if (tilesizy_) delete [] tilesizy_;
		if (picanm_) delete [] picanm_;
		if (insert_) delete [] insert_;
		
		insert_ = 0;
		insertlen_ = 0;
	}
	
	void load()
	{
		ifstream infile(filename_.c_str(), ios::in | ios::binary);
		int i, ntiles;
		
		if (infile.is_open()) {
			do {
				if (readLong(infile) != 1) {
					break;
				}
				readLong(infile);	// skip the numtiles
				dispose();
				
				localtilestart_ = readLong(infile);
				localtileend_   = readLong(infile);
				ntiles = localtileend_ - localtilestart_ + 1;
				
				tilesizx_ = new short[ntiles];
				tilesizy_ = new short[ntiles];
				picanm_   = new long[ntiles];
				
				for (i = 0; i < ntiles; i++) {
					tilesizx_[i] = readShort(infile);
				}
				for (i = 0; i < ntiles; i++) {
					tilesizy_[i] = readShort(infile);
				}
				for (i = 0; i < ntiles; i++) {
					picanm_[i] = readLong(infile);
				}
				
			} while (0);
			
			infile.close();
		}
	}
	
public:
	ARTFile(string filename)
	 : filename_(filename), localtilestart_(0), localtileend_(-1),
	   tilesizx_(0), tilesizy_(0), picanm_(0),
	   markprelength_(0), markskiplength_(0), markpostlength_(0),
	   insert_(0), insertlen_(0)
	{
		load();
	}
	
	~ARTFile()
	{
		dispose();
	}

	/**
	 * Sets up for an empty file
	 * @param start the starting tile
	 * @param ntiles the number of tiles total
	 */
	void init(int start, int ntiles)
	{
		dispose();
		
		localtilestart_ = start;
		localtileend_ = start + ntiles - 1;
		tilesizx_ = new short[ntiles];
		tilesizy_ = new short[ntiles];
		picanm_   = new long[ntiles];
		
		memset(tilesizx_, 0, sizeof(short)*ntiles);
		memset(tilesizy_, 0, sizeof(short)*ntiles);
		memset(picanm_, 0, sizeof(long)*ntiles);
		
		markprelength_ = 0;
		markskiplength_ = 0;
		markpostlength_ = 0;
		insert_ = 0;
		insertlen_ = 0;
	}
	
	/**
	 * Returns the number of tiles in the loaded file
	 * @return 0 means no file loaded
	 */
	int getNumTiles()
	{
		return (localtileend_ - localtilestart_ + 1);
	}
	
	int getFirstTile()
	{
		return localtilestart_;
	}
	
	int getLastTile()
	{
		return localtileend_;
	}
	
	void removeTile(int tile)
	{
		int i, end;
		
		if (tile < localtilestart_ || tile > localtileend_) {
			return;
		}
		
		end   = localtileend_ - tile;
		tile -= localtilestart_;
		
		markprelength_ = markpostlength_ = 0;
		
		for (i = 0; i < tile; i++) {
			markprelength_ += tilesizx_[i] * tilesizy_[i];
		}
		markskiplength_ = tilesizx_[tile] * tilesizy_[tile];
		for (i = tile + 1; i <= end; i++) {
			markpostlength_ += tilesizx_[i] * tilesizy_[i];
		}
		
		tilesizx_[tile] = tilesizy_[tile] = 0;
	}
	
	void replaceTile(int tile, char * replace, int replacelen)
	{
		if (tile < localtilestart_ || tile > localtileend_) {
			return;
		}

		removeTile(tile);

		insert_ = replace;
		insertlen_ = replacelen;
	}
	
	void setTileSize(int tile, int x, int y)
	{
		if (tile < localtilestart_ || tile > localtileend_) {
			return;
		}

		tile -= localtilestart_;
		tilesizx_[tile] = x;
		tilesizy_[tile] = y;
	}
	
	void setXOfs(int tile, int x)
	{
		if (tile < localtilestart_ || tile > localtileend_) {
			return;
		}
		
		tile -= localtilestart_;
		picanm_[tile] &= ~(255<<8);
		picanm_[tile] |= ((long)((unsigned char)x) << 8);
	}
	
	void setYOfs(int tile, int y)
	{
		if (tile < localtilestart_ || tile > localtileend_) {
			return;
		}
		
		tile -= localtilestart_;
		picanm_[tile] &= ~(255<<16);
		picanm_[tile] |= ((long)((unsigned char)y) << 16);
	}
	
	void setAnimType(int tile, int type)
	{
		if (tile < localtilestart_ || tile > localtileend_) {
			return;
		}
		
		tile -= localtilestart_;
		picanm_[tile] &= ~(3<<6);
		picanm_[tile] |= ((long)(type&3) << 6);
	}
	
	void setAnimFrames(int tile, int frames)
	{
		if (tile < localtilestart_ || tile > localtileend_) {
			return;
		}
		
		tile -= localtilestart_;
		picanm_[tile] &= ~(63);
		picanm_[tile] |= ((long)(frames&63));
	}
	
	void setAnimSpeed(int tile, int speed)
	{
		if (tile < localtilestart_ || tile > localtileend_) {
			return;
		}
		
		tile -= localtilestart_;
		picanm_[tile] &= ~(15<<24);
		picanm_[tile] |= ((long)(speed&15) << 24);
	}
	
	int write()
	{
		string tmpfilename(filename_ + ".arttool");
		ofstream outfile(tmpfilename.c_str(), ios::out | ios::trunc | ios::binary);
		ifstream infile(filename_.c_str(), ios::in | ios::binary);
		int i, left;
		char blk[4096];
		
		if (!infile.is_open() && (markprelength_ > 0 || markskiplength_ > 0 || markpostlength_ > 0)) {
			return -1;	// couldn't open the original file for copying
		} else if (infile.is_open()) {
			// skip to the start of the existing ART data
			int ofs = 4+4+4+4+(2+2+4)*(localtileend_-localtilestart_+1);
			infile.seekg(ofs, ios::cur);
		}
		
		// write a header to the temporary file
		writeLong(outfile, 1);	// version
		writeLong(outfile, 0);	// numtiles
		writeLong(outfile, localtilestart_);
		writeLong(outfile, localtileend_);
		for (int i = 0; i < localtileend_ - localtilestart_ + 1; i++) {
			writeShort(outfile, tilesizx_[i]);
		}
		for (int i = 0; i < localtileend_ - localtilestart_ + 1; i++) {
			writeShort(outfile, tilesizy_[i]);
		}
		for (int i = 0; i < localtileend_ - localtilestart_ + 1; i++) {
			writeLong(outfile, picanm_[i]);
		}
		
		// copy the existing leading tile data to be kept
		left = markprelength_;
		while (left > 0) {
			i = left;
			if (i > sizeof(blk)) {
				i = sizeof(blk);
			}
			infile.read(blk, i);
			outfile.write(blk, i);
			left -= i;
		}
		
		// insert the replacement data
		if (insertlen_ > 0) {
			outfile.write(insert_, insertlen_);
		}
		
		if (markskiplength_ > 0) {
			infile.seekg(markskiplength_, ios::cur);
		}
		
		// copy the existing trailing tile data to be kept
		left = markpostlength_;
		while (left > 0) {
			i = left;
			if (i > sizeof(blk)) {
				i = sizeof(blk);
			}
			infile.read(blk, i);
			outfile.write(blk, i);
			left -= i;
		}
		
		// close our files
		if (infile.is_open()) {
			infile.close();
		}
		outfile.close();
		
		// replace it with the new one
		unlink(filename_.c_str());
		rename(tmpfilename.c_str(), filename_.c_str());
	}
};

class Operation {
protected:
	string makefilename(int n)
	{
		string filename("tilesXXX.art");
		filename[5] = '0' + (n / 100) % 10;
		filename[6] = '0' + (n / 10) % 10;
		filename[7] = '0' + (n / 1) % 10;
		return filename;
	}
	
public:
	typedef enum {
		NO_ERROR = 0,
		ERR_BAD_OPTION = 1,
		ERR_BAD_VALUE = 2,
		ERR_TOO_MANY_PARAMS = 3,
		ERR_NO_ART_FILE = 4,
	} Result;
	
	/**
	 * Sets an option
	 * @param opt the option name
	 * @param value the option value
	 * @return a value from the Result enum
	 */
	virtual Result setOption(string opt, string value) = 0;
	
	/**
	 * Sets a parameter from the unnamed sequence
	 * @param number the parameter number
	 * @param value the parameter value
	 * @return a value from the Result enum
	 */
	virtual Result setParameter(int number, string value) = 0;
	
	/**
	 * Do the operation
	 * @return a value from the Result enum
	 */
	virtual Result perform() = 0;
};

class CreateOp : public Operation {
private:
	int filen_, offset_, ntiles_;
public:
	CreateOp() : filen_(0), offset_(0), ntiles_(256) { }
	
	virtual Result setOption(string opt, string value)
	{
		if (opt == "f") {
			filen_ = atoi(value.c_str());
		} else if (opt == "o") {
			offset_ = atoi(value.c_str());
		} else if (opt == "n") {
			ntiles_ = atoi(value.c_str());
		} else {
			return ERR_BAD_OPTION;
		}
		if (filen_ < 0 || filen_ > 999) {
			return ERR_BAD_VALUE;
		}
		if (offset_ < 0) {
			return ERR_BAD_VALUE;
		}
		if (ntiles_ < 1) {
			return ERR_BAD_VALUE;
		}
		return NO_ERROR;
	}
	
	virtual Result setParameter(int number, string value)
	{
		return ERR_TOO_MANY_PARAMS;
	}
	
	virtual Result perform()
	{
		ARTFile art(makefilename(filen_));
		
		art.init(offset_, ntiles_);
		art.write();
		
		return NO_ERROR;
	}
};

class AddTileOp : public Operation {
private:
	int xofs_, yofs_;
	int animframes_, animtype_, animspeed_;
	int tilenum_;
	string filename_;
public:
	AddTileOp()
	 : xofs_(0), yofs_(0),
	   animframes_(0), animtype_(0), animspeed_(0),
	   tilenum_(-1), filename_("")
	{ }
	
	virtual Result setOption(string opt, string value)
	{
		if (opt == "x") {
			xofs_ = atoi(value.c_str());
		} else if (opt == "y") {
			yofs_ = atoi(value.c_str());
		} else if (opt == "ann") {
			animframes_ = atoi(value.c_str());
		} else if (opt == "ant") {
			animtype_ = atoi(value.c_str());
		} else if (opt == "ans") {
			animspeed_ = atoi(value.c_str());
		} else {
			return ERR_BAD_OPTION;
		}
		if (animtype_ < 0 || animtype_ > 3) {
			return ERR_BAD_VALUE;
		}
		if (animspeed_ < 0 || animspeed_ > 15) {
			return ERR_BAD_VALUE;
		}
		if (animframes_ < 0 || animframes_ > 63) {
			return ERR_BAD_VALUE;
		}
		return NO_ERROR;
	}
	
	virtual Result setParameter(int number, string value)
	{
		switch (number) {
			case 0:
				tilenum_ = atoi(value.c_str());
				return NO_ERROR;
			case 1:
				filename_ = value;
				return NO_ERROR;
			default:
				return ERR_TOO_MANY_PARAMS;
		}
	}
	
	virtual Result perform()
	{
		int tilesperfile = 0, nextstart = 0;
		int filenum = 0;
		
		// open the first art file to get the file size used by default
		{
			ARTFile art(makefilename(0));
			tilesperfile = art.getNumTiles();
			if (tilesperfile == 0) {
				return ERR_NO_ART_FILE;
			}
		}
	
		// open art files until we find one that encompasses the range we need
		// and when we find it, make the change
		for (filenum = 0; filenum < 1000; filenum++) {
			ARTFile art(makefilename(filenum));
			bool dirty = false, done = false;
			
			if (art.getNumTiles() == 0) {
				// no file exists, so we treat it as though it does
				art.init(nextstart, tilesperfile);
				dirty = true;
			}
			
			if (tilenum_ >= art.getFirstTile() && tilenum_ <= art.getLastTile()) {
				char *tmpdata = new char[4];
				tmpdata[0] = 64;
				tmpdata[1] = 66;
				tmpdata[2] = 68;
				tmpdata[3] = 70;
				art.replaceTile(tilenum_, tmpdata, 4);
				art.setTileSize(tilenum_, 2, 2);
				art.setXOfs(tilenum_, xofs_);
				art.setYOfs(tilenum_, yofs_);
				art.setAnimFrames(tilenum_, animframes_);
				art.setAnimSpeed(tilenum_, animspeed_);
				art.setAnimType(tilenum_, animtype_);
				done = true;
				dirty = true;
			}
			
			nextstart += art.getNumTiles();

			if (dirty) {
				art.write();
			}
			if (done) {
				break;
			}
		}
		
		return NO_ERROR;
	}
};

int main(int argc, char ** argv)
{
	int showusage = 0;
	Operation * oper = 0;
	
	if (argc < 2) {
		showusage = 1;
	} else {
		string opt(argv[1]);
		string value;
		
		// create the option handler object according to the first param
		if (opt == "create") {
			oper = new CreateOp;
		} else if (opt == "addtile") {
			oper = new AddTileOp;
		} else if (opt == "rmtile") {
		} else if (opt == "tileprop") {
		} else {
			showusage = 2;
		}
		
		// apply the command line options given
		if (oper) {
			int unnamedParm = 0;
			for (int i = 2; i < argc && !showusage; i++) {
				if (argv[i][0] == '-') {
					opt = string(argv[i]).substr(1);
					if (i+1 >= argc) {
						showusage = 2;
						break;
					}
					value = string(argv[i+1]);
					i++;
					
					switch (oper->setOption(opt, value)) {
						case Operation::NO_ERROR: break;
						default: showusage = 2; break;
					}
				} else {
					value = string(argv[i]);
					switch (oper->setParameter(unnamedParm, value)) {
						case Operation::NO_ERROR: break;
						default: showusage = 2; break;
					}
					unnamedParm++;
				}
			}
		}
	}
	
	if (showusage) {
		usage();
		if (oper) delete oper;
		return (showusage - 1);
	} else if (oper) {
		Operation::Result r;
		
		r = oper->perform();
		delete oper;
		
		switch (r) {
			case Operation::NO_ERROR: return 0;
			default: return 1;
		}
	}
	
	return 0;
}