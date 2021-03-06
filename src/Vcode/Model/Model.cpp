#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <locale>
#include<Model/Model.h>
#include<qdebug.h>
#pragma comment(lib,"libtesseract304d.lib")
#pragma comment(lib,"liblept171d.lib")

void Model::processPicture(int grayType, int removet, int binaryt, int denoiser) {
	try {
		if (grayType < 0) {
			throw QException("Please choose one gray type!");
		}
		if (m.empty()) {
			throw QException("Open one image first!");
		}
		//begin graym
		m.copyTo(graym);
		int nr = m.rows; // number of rows
		int nc = m.cols * m.channels(); // total number of elements per line
		switch (grayType) {
			case GrayType::GRAY_AVERAGE: {
				for (int j = 0; j < nr; j++) {
					uchar* data = m.ptr<uchar>(j);
					uchar* data1 = graym.ptr<uchar>(j);
					for (int i = 0; i < nc; i += 3) {
						data1[i] = data1[i + 1] = data1[i + 2] = (data[i] + data[i + 1] + data[i + 2]) / 3;
					}
				}
				break;
			}
			case GrayType::GRAY_MAX: {
				for (int j = 0; j < nr; j++) {
					uchar* data = m.ptr<uchar>(j);
					uchar* data1 = graym.ptr<uchar>(j);
					for (int i = 0; i < nc; i += 3) {
						uchar maxc = max(max(data[i], data[i + 1]), data[i + 2]);
						data1[i] = data1[i + 1] = data1[i + 2] = maxc;
					}
				}
				break;
			}
			case GrayType::GRAY_WEIGHTAVE: {
				for (int j = 0; j < nr; j++) {
					uchar* data = m.ptr<uchar>(j);
					uchar* data1 = graym.ptr<uchar>(j);
					for (int i = 0; i < nc; i += 3) {
						data1[i] = data1[i + 1] = data1[i + 2] = 0.3*data[i + 2] + 0.59*data[i + 1] + 0.11*data[i];
						//i Blue; i+1 Green; i+2 Red
					}
				}
				break;
			}
		}
		//end graym
		if (graym.empty()) {
			throw QException("Image can not be transformed to gray type!");
		}
		//begin remove background
		graym.copyTo(removeBGm);
		for (int j = 0; j < nr; j++) {
			uchar* data = removeBGm.ptr<uchar>(j);

			for (int i = 0; i < nc; i++) {
				data[i] = (data[i] > removet) ? 255 : data[i];
			}
		}
		if (removeBGm.empty()) {
			throw QException("Image can not be removed background!");
		}
		//end remove bg
		//begin binary
		removeBGm.copyTo(binarym);
		for (int j = 0; j < nr; j++) {
			uchar* data = binarym.ptr<uchar>(j);

			for (int i = 0; i < nc; i++) {
				data[i] = (data[i] > binaryt) ? 255 : 0;
				//0 black, 255 white
			}
		}
		if (binarym.empty()) {
			throw QException("Image can not be transformed to binary type!");
		}
		//end binary
		//begin denoise
		binarym.copyTo(denoisem);
		cv::Mat tmp;
		denoisem.copyTo(tmp);
		int r = denoiser;
		uchar** datas = new uchar*[nr];
		//erosion
		for (int i = 0; i < nr; i++) {
			datas[i] = binarym.ptr<uchar>(i);
		}

		for (int j = r; j < nr - r; j++) {
			uchar* desData = tmp.ptr<uchar>(j);

			for (int i = r; i < nc - r; i++) {
				//0 black, 255 white, all 0, then 0
				bool flag = true;
				for (int m = -r; m <= r; m++) {
					for (int n = -r; n <= r; n++) {
						if (datas[j + m][i + n]) {
							flag = false;
							break;
						}
					}
					if (!flag) break;
				}
				if (flag) {
					desData[i] = 0;
				}
				else {
					desData[i] = 255;
				}
			}
		}
		tmp.copyTo(denoisem);
		//dalition
		for (int i = 0; i < nr; i++) {
			datas[i] = denoisem.ptr<uchar>(i);
		}

		for (int j = r; j < nr - r; j++) {
			uchar* desData = tmp.ptr<uchar>(j);

			for (int i = r; i < nc - r; i++) {
				//0 black, 255 white, all 255, then 255
				bool flag = false;
				for (int m = -r; m <= r; m++) {
					for (int n = -r; n <= r; n++) {
						if (!datas[j + m][i + n]) {
							flag = true;
							break;
						}
					}
					if (flag) break;
				}
				if (flag) {
					desData[i] = 0;
				}
				else {
					desData[i] = 255;
				}
			}
		}
		tmp.copyTo(denoisem);
		if (denoisem.empty()) {
			throw QException("Image can not be denoised!");
		}
		cv::imwrite("denoise.jpg", denoisem);
		//end denoise
		string s = "process";
		this->notify(s);
	}
	catch (QException& E) {
		e = E;
		this->notify(false);
	}
}
wstring Model::UTF8ToUnicode(const string& str) {
	int  len = 0;
	len = str.length();
	int  unicodeLen = ::MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
	wchar_t* pUnicode;
	pUnicode = new wchar_t[unicodeLen + 1];
	memset(pUnicode, 0, (unicodeLen + 1) * sizeof(wchar_t));
	::MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, (LPWSTR)pUnicode, unicodeLen);
	wstring rt;
	rt = (wchar_t*)pUnicode;
	delete pUnicode;
	return rt;
}
void Model::solvePicture() {

	try {
		if (denoisem.empty()) {
			throw QException("Denoise image first!");
		}
		tesseract::TessBaseAPI *api = new tesseract::TessBaseAPI();
		// Initialize tesseract-ocr with English, without specifying tessdata path
		if (api->Init(NULL, "eng+chi_sim+normal")) {
			throw QException("Could not initialize tesseract!");
			exit(1);
		}

		// Open input image with leptonica library
		Pix *image = pixRead("denoise.jpg");
		if (image == nullptr) {
			throw QException("Denoised image can not be found!");
		}
		api->SetImage(image);
		// Get OCR result

		res = api->GetUTF8Text();

		api->End();
		pixDestroy(&image);
		string s = "result";
		this->notify(s);
	}
	catch (QException& E) {
		e = E;
		this->notify(false);
	}
}
void Model::loadPicture(const string& path) {
	m = cv::imread(path, 1);
	if (m.empty()) {
		e.setErrorMes("Load picture failed!");
		this->notify(false);
	}
	else {
		string s = "image";
		this->notify(s);
	}
}
void Model::saveResult(string savePath) {
	try {
		if (savePath.empty()) {
			throw QException("Path is empty!");
		}
		if (res.empty()) {
			throw QException("Result does not exist!");
		}
		ofstream fout(savePath);

		if (fout.bad()) {
			throw QException("File can not be created!");
		}
		fout << res;
	}
	catch (QException& E) {
		e = E;
		notify(false);
	}
}
cv::Mat& Model::getMat() {
	return m;
}
cv::Mat& Model::getGrayMat() {
	return graym;
}
cv::Mat& Model::getDenoiseMat() {
	return denoisem;
}
cv::Mat& Model::getRemoveBGMat() {
	return removeBGm;
}
cv::Mat& Model::getBinaryMat() {
	return binarym;
}
QException& Model::getException() {
	return e;
}

QException::QException(string s) :errorMessage(s) {}
void QException::setErrorMes(string s) {
	errorMessage = s;
}
string QException::getErrorMes() {
	return errorMessage;
}