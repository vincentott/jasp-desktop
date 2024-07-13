//
// Copyright (C) 2013-2017 University of Amsterdam
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#include "jasprcpp.h"
#include <fstream>
#include "tempfiles.h"

static const	std::string NullString			= "null";
static			std::string lastErrorMessage	= "";

RInside							*rinside;
ReadDataSetCB					readDataSetCB;
RunCallbackCB					runCallbackCB;
ReadADataSetCB					readFullDataSetCB,
								readFullFilteredDataSetCB,
								readFilterDataSetCB;
ReadDataColumnNamesCB			readDataColumnNamesCB;
RequestTempFileNameCB			requestTempFileNameCB,
								requestSpecificFileNameCB;
RequestTempRootNameCB			requestTempRootNameCB;
ReadDataSetDescriptionCB		readDataSetDescriptionCB;
RequestPredefinedFileSourceCB	requestStateFileSourceCB,
								requestJaspResultsFileSourceCB;

DataSetRowCount					dataSetRowCount;
CreateColumn					dataSetCreateColumn;
DeleteColumn					dataSetDeleteColumn;
GetColumnType					dataSetGetColumnType;
SetColumnDataAndType			dataSetColumnDataAndType;
GetColumnAnalysisId				dataSetGetColumnAnalysisId;

EnDecodeDef						encodeColumnName,
								decodeColumnName,
								encodeAllColumnNames,
								decodeAllColumnNames;

ShouldEnDecodeDef				shouldEncodeColumnName,
								shouldDecodeColumnName;

getColNames						getAllColumnNames;

static logFlushDef				_logFlushFunction		= nullptr;
static logWriteDef				_logWriteFunction		= nullptr;
static sendFuncDef				_sendToDesktop			= nullptr;
static pollMessagesFuncDef		_pollMessagesFunction	= nullptr;
static systemDef				_systemFunc				= nullptr;
static libraryFixerDef			_libraryFixerFunc		= nullptr;
static std::string				_R_HOME = "";

bool shouldCrashSoon = false; //Simply here to allow a developer to force a crash

//Ugly hack to work around windows messing up environment variables when local+codepage+utf8
//Might not be necessary anymore due to the active codepage being set to utf8 now
extern char * R_TempDir;

extern "C" {
void STDCALL jaspRCPP_init(const char* buildYear, const char* version, RBridgeCallBacks* callbacks,
	sendFuncDef sendToDesktopFunction, pollMessagesFuncDef pollMessagesFunction,
	logFlushDef logFlushFunction, logWriteDef logWriteFunction, systemDef systemFunc,
	libraryFixerDef libraryFixerFunc, const char* resultFont, const char * tempDir)
{
	_logFlushFunction		= logFlushFunction;
	_logWriteFunction		= logWriteFunction;
	_sendToDesktop			= sendToDesktopFunction;
	_pollMessagesFunction	= pollMessagesFunction;
	_systemFunc				= systemFunc;
	_libraryFixerFunc		= libraryFixerFunc;

	jaspRCPP_logString("Creating RInside.\n");

	rinside = new RInside();
	R_TempDir = (char*)tempDir;
	
	RInside &rInside = rinside->instance();

	requestJaspResultsFileSourceCB				= callbacks->requestJaspResultsFileSourceCB;
	dataSetGetColumnAnalysisId					= callbacks->dataSetGetColumnAnalysisId;
	dataSetColumnDataAndType					= callbacks->dataSetColumnAsDataAndType;
	requestSpecificFileNameCB					= callbacks->requestSpecificFileNameCB;
	readFullFilteredDataSetCB					= callbacks->readFullFilteredDataSetCB;
	requestStateFileSourceCB					= callbacks->requestStateFileSourceCB;
	readDataSetDescriptionCB					= callbacks->readDataSetDescriptionCB;
	requestTempRootNameCB						= callbacks->requestTempRootNameCB;
	requestTempFileNameCB						= callbacks->requestTempFileNameCB;
	readDataColumnNamesCB						= callbacks->readDataColumnNamesCB;
	dataSetGetColumnType						= callbacks->dataSetGetColumnType;
	dataSetCreateColumn							= callbacks->dataSetCreateColumn;
	dataSetDeleteColumn							= callbacks->dataSetDeleteColumn;
	readFilterDataSetCB							= callbacks->readFilterDataSetCB;
	readFullDataSetCB							= callbacks->readFullDataSetCB;
	dataSetRowCount								= callbacks->dataSetRowCount;
	runCallbackCB								= callbacks->runCallbackCB;
	readDataSetCB								= callbacks->readDataSetCB;
	getAllColumnNames							= callbacks->columnNames;
	shouldEncodeColumnName						= callbacks->shouldEncode;
	shouldDecodeColumnName						= callbacks->shouldDecode;
	encodeAllColumnNames						= callbacks->encoderAll;
	decodeAllColumnNames						= callbacks->decoderAll;
	encodeColumnName							= callbacks->encoder;
	decodeColumnName							= callbacks->decoder;

	// TODO: none of this should pollute the global environment.
	rInside[".setLog"]							= Rcpp::InternalFunction(&jaspRCPP_setLog);
	rInside[".setRError"]						= Rcpp::InternalFunction(&jaspRCPP_setRError);
	rInside[".crashPlease"]						= Rcpp::InternalFunction(&jaspRCPP_crashPlease);
	rInside[".setRWarning"]						= Rcpp::InternalFunction(&jaspRCPP_setRWarning);
	rInside[".runSeparateR"]					= Rcpp::InternalFunction(&jaspRCPP_RunSeparateR);
	rInside[".returnString"]					= Rcpp::InternalFunction(&jaspRCPP_returnString);
	rInside[".columnIsScale"]					= Rcpp::InternalFunction(&jaspRCPP_columnIsScale);
	rInside[".callbackNative"]					= Rcpp::InternalFunction(&jaspRCPP_callbackSEXP);
	rInside[".dataSetRowCount"]					= Rcpp::InternalFunction(&jaspRCPP_dataSetRowCount);
	rInside[".returnDataFrame"]					= Rcpp::InternalFunction(&jaspRCPP_returnDataFrame);
	rInside[".columnIsOrdinal"]					= Rcpp::InternalFunction(&jaspRCPP_columnIsOrdinal);
	rInside[".columnIsNominal"]					= Rcpp::InternalFunction(&jaspRCPP_columnIsNominal);
	rInside[".encodeColNamesLax"]				= Rcpp::InternalFunction(&jaspRCPP_encodeAllColumnNames);
	rInside[".decodeColNamesLax"]				= Rcpp::InternalFunction(&jaspRCPP_decodeAllColumnNames);
	rInside[".encodeColNamesStrict"]			= Rcpp::InternalFunction(&jaspRCPP_encodeColumnNameRcpp);
	rInside[".decodeColNamesStrict"]			= Rcpp::InternalFunction(&jaspRCPP_decodeColumnNameRcpp);
	rInside[".setColumnDataAsScale"]			= Rcpp::InternalFunction(&jaspRCPP_setColumnDataAsScale);
	rInside[".readFullDatasetToEnd"]			= Rcpp::InternalFunction(&jaspRCPP_readFullDataSet);
	rInside[".allColumnNamesDataset"]			= Rcpp::InternalFunction(&jaspRCPP_allColumnNamesDataset);
	rInside[".readDatasetToEndNative"]			= Rcpp::InternalFunction(&jaspRCPP_readDataSetSEXP);
	rInside[".readFilterDatasetToEnd"]			= Rcpp::InternalFunction(&jaspRCPP_readFilterDataSet);
	rInside[".setColumnDataAsOrdinal"]			= Rcpp::InternalFunction(&jaspRCPP_setColumnDataAsOrdinal);
	rInside[".setColumnDataAsNominal"]			= Rcpp::InternalFunction(&jaspRCPP_setColumnDataAsNominal);
	rInside[".readDataSetHeaderNative"]			= Rcpp::InternalFunction(&jaspRCPP_readDataSetHeaderSEXP);
	rInside[".createCaptureConnection"]			= Rcpp::InternalFunction(&jaspRCPP_CreateCaptureConnection);
	rInside[".postProcessLibraryModule"]		= Rcpp::InternalFunction(&jaspRCPP_postProcessLocalPackageInstall);
	rInside[".requestTempFileNameNative"]		= Rcpp::InternalFunction(&jaspRCPP_requestTempFileNameSEXP);
	rInside[".requestTempRootNameNative"]		= Rcpp::InternalFunction(&jaspRCPP_requestTempRootNameSEXP);
	rInside[".requestStateFileNameNative"]		= Rcpp::InternalFunction(&jaspRCPP_requestStateFileNameSEXP);
	rInside[".readFullFilteredDatasetToEnd"]	= Rcpp::InternalFunction(&jaspRCPP_readFullFilteredDataSet);
	rInside[".requestSpecificFileNameNative"]	= Rcpp::InternalFunction(&jaspRCPP_requestSpecificFileNameSEXP);
	
	jaspRCPP_logString("Creating Output sink.\n");
	rInside[".outputSink"]						= jaspRCPP_CreateCaptureConnection();

	rInside.parseEvalQNT("sink(.outputSink); print('.outputSink initialized!'); sink();");
	Rcpp::RObject sinkObj = rInside[".outputSink"];
	//jaspRCPP_logString(sinkObj.isNULL() ? "sink is null\n" : !sinkObj.isObject() ? " sink is not object\n" : sinkObj.isS4() ? "sink is s4\n" : "sink is obj but not s4\n");

	rInside.parseEvalQNT("sink(.outputSink); print(.libPaths()); sink();");

	// initialize everything unrelated to jaspBase
	static const char *baseCitationFormat	= "JASP Team (%s). JASP (Version %s) [Computer software].";
	char baseCitation[200];
	snprintf(baseCitation, 200, baseCitationFormat, buildYear, version);
	rInside[".baseCitation"]	= baseCitation;
	rInside[".jaspVersion"]		= version;

	rInside[".baseCitation"]					= baseCitation;
	rInside[".numDecimals"]						= 3;
	rInside[".fixedDecimals"]					= false;
	rInside[".normalizedNotation"]				= true;
	rInside[".exactPValues"]					= false;
	rInside[".resultFont"]						= "Arial";
	rInside[".imageBackground"]					= "transparent";
	rInside[".ppi"]								= 300;

	jaspRCPP_parseEvalQNT("library(methods)");

	_R_HOME = jaspRCPP_parseEvalStringReturn("R.home('')");
	jaspRCPP_logString("R_HOME is: " + _R_HOME + "\n");

}

void STDCALL jaspRCPP_init_jaspBase()
{

	jaspRCPP_logString("Start initializing jaspBase\n");

	//XPtr doesnt like it if it can't run a finalizer so here are some dummy variables:
	static logFuncDef				_logFuncDef					= jaspRCPP_logString;
	static getColumnTypeFuncDef		_getColumnTypeFuncDef		= jaspRCPP_getColumnType;
	static getColumnExistsFDef		_getColumnExistsFuncDef		= jaspRCPP_getColumnExists;
	static createColumnFuncDef		_createColumnFuncDef		= jaspRCPP_createColumn;
	static deleteColumnFuncDef		_deleteColumnFuncDef		= jaspRCPP_deleteColumn;
	static getColumnAnIdFuncDef		_getColumnAnIdFuncDef		= jaspRCPP_getColumnAnalysisId;
	static setColumnDataFuncDef		_setColumnDataAsScale		= jaspRCPP_setColumnDataAsScale;
	static setColumnDataFuncDef		_setColumnDataAsOrdinal		= jaspRCPP_setColumnDataAsOrdinal;
	static setColumnDataFuncDef		_setColumnDataAsNominal		= jaspRCPP_setColumnDataAsNominal;
	static shouldEnDecodeFuncDef	_shouldEncodeColumnName		= jaspRCPP_shouldEncodeColumnName;
	static shouldEnDecodeFuncDef	_shouldDecodeColumnName		= jaspRCPP_shouldDecodeColumnName;
	
	static enDecodeFuncDef			_encodeColumnName			= jaspRCPP_encodeColumnName;
	static enDecodeFuncDef			_decodeColumnName			= jaspRCPP_decodeColumnName;

	RInside &rInside = rinside->instance();

	rInside[".logString"]						= Rcpp::XPtr<logFuncDef>(			& _logFuncDef);
	rInside[".createColumn"]					= Rcpp::XPtr<createColumnFuncDef>(	& _createColumnFuncDef);
	rInside[".deleteColumn"]					= Rcpp::XPtr<deleteColumnFuncDef>(	& _deleteColumnFuncDef);
	rInside[".getColumnType"]					= Rcpp::XPtr<getColumnTypeFuncDef>(	& _getColumnTypeFuncDef);
	rInside[".getColumnExists"]					= Rcpp::XPtr<getColumnExistsFDef>(	& _getColumnExistsFuncDef);
	rInside[".getColumnAnalysisId"]				= Rcpp::XPtr<getColumnAnIdFuncDef>(	& _getColumnAnIdFuncDef);
	rInside[".sendToDesktopFunction"]			= Rcpp::XPtr<sendFuncDef>(			&  _sendToDesktop);
	rInside[".pollMessagesFunction"]			= Rcpp::XPtr<pollMessagesFuncDef>(	&  _pollMessagesFunction);

	rInside[".setColumnDataAsScalePtr"]			= Rcpp::XPtr<setColumnDataFuncDef>(	& _setColumnDataAsScale);
	rInside[".setColumnDataAsOrdinalPtr"]		= Rcpp::XPtr<setColumnDataFuncDef>(	& _setColumnDataAsOrdinal);
	rInside[".setColumnDataAsNominalPtr"]		= Rcpp::XPtr<setColumnDataFuncDef>(	& _setColumnDataAsOrdinal);
	rInside[".shouldEncodeColName"]				= Rcpp::XPtr<shouldEnDecodeFuncDef>(& _shouldEncodeColumnName);
	rInside[".shouldDecodeColName"]				= Rcpp::XPtr<shouldEnDecodeFuncDef>(& _shouldDecodeColumnName);
	rInside[".encodeColName"]					= Rcpp::XPtr<enDecodeFuncDef>(		& _encodeColumnName);
	rInside[".decodeColName"]					= Rcpp::XPtr<enDecodeFuncDef>(		& _decodeColumnName);

	//Pass a whole bunch of pointers to jaspBase
	jaspRCPP_parseEvalQNT("jaspBase:::setColumnFuncs(		.setColumnDataAsScalePtr, .setColumnDataAsOrdinalPtr, .setColumnDataAsNominalPtr, .getColumnType, .getColumnAnalysisId, .createColumn, .deleteColumn, .getColumnExists, .encodeColName, .decodeColName, .shouldEncodeColName, .shouldDecodeColName)");
	jaspRCPP_parseEvalQNT("jaspBase:::setJaspLogFunction(	.logString					)");
	jaspRCPP_parseEvalQNT("jaspBase:::setSendFunc(			.sendToDesktopFunction)");
	jaspRCPP_parseEvalQNT("jaspBase:::setPollMessagesFunc(	.pollMessagesFunction)");
	jaspRCPP_parseEvalQNT("jaspBase:::setBaseCitation(		.baseCitation)");
	jaspRCPP_parseEvalQNT("jaspBase:::setInsideJasp()");
	jaspRCPP_parseEvalQNT("jaspBase:::registerFonts()");

	//Load it
	jaspRCPP_logString("Initializing jaspBase.\n");
	jaspRCPP_parseEvalQNT("library(jaspBase)");

//	if we have a separate engine for each module then we should move these kind of hacks to the .onAttach() of each module (instead of loading BayesFactor when Descriptives is requested).
//	jaspRCPP_logString("initEnvironment().\n");
//	jaspRCPP_parseEvalQNT("initEnvironment()");
	
	jaspRCPP_logString("initializeDoNotRemoveList().\n");
	jaspRCPP_parseEvalQNT("jaspBase:::.initializeDoNotRemoveList()");

	jaspRCPP_logString("Finished initializing jaspBase.\n");

}

void STDCALL jaspRCPP_junctionHelper(bool collectNotRestore, const char * modulesFolder, const char * linkFolder, const char * junctionsFilePath)
{
	rinside = new RInside();
	RInside &rInside = rinside->instance();
	
	std::cout << "RInside created, now about to " << (collectNotRestore ? "collect" :  "recreate") << " Modules junctions in renv-cache" << std::endl;
	
	rinside->parseEvalQNT("source('Modules/Tools/symlinkTools.R')");
	rInside["modulesFolder"] = modulesFolder;
	rInside["symFolder"] = linkFolder;
	rInside["junctionsFilePath"] = junctionsFilePath;
	rinside->parseEvalQNT(".libPaths( c( paste0( modulesFolder, 'Tools/junction_bootstrap_library' )  , .libPaths() ) )");
	
	if(collectNotRestore)	rinside->parseEvalQNT("collectAndStoreJunctions(modulesFolder)");
	else					rinside->parseEvalQNT("restoreModulesIfNeeded( modulesFolder, symFolder, junctionsFilePath)");
}

void STDCALL jaspRCPP_purgeGlobalEnvironment()
{
	jaspRCPP_parseEvalQNT("jaspBase:::.cleanEngineMemory()", false);
}

void _setJaspResultsInfo(int analysisID, int analysisRevision, bool developerMode)
{
	jaspRCPP_parseEvalQNT(
		"jaspBase:::setResponseData(" + std::to_string(analysisID) +", " + std::to_string(analysisRevision) + ");\n" +
		"jaspBase:::setDeveloperMode(" + (developerMode ? "TRUE" : "FALSE") + ")"
	);

	std::string root, relativePath;

	if(!jaspRCPP_requestJaspResultsRelativeFilePath(root, relativePath))
		throw std::runtime_error("Did not receive a valid filename to store jaspResults.json at.");

	jaspRCPP_parseEvalQNT("jaspBase:::setSaveLocation(\"" + root + "\", \"" + relativePath + "\");");

	std::string specificFilename = jaspRCPP_parseEvalStringReturn("jaspBase:::writeSealFilename()");
	if(!jaspRCPP_requestSpecificRelativeFilePath(specificFilename, root, relativePath))
			throw std::runtime_error("Did not receive a valid filename to store jaspResults write seal at.");

	jaspRCPP_parseEvalQNT("jaspBase:::setWriteSealLocation(\"" + root + "\", \"" + relativePath + "\");");
}

void STDCALL jaspRCPP_setDecimalSettings(int numDecimals, bool fixedDecimals, bool normalizedNotation, bool exactPValues)
{
	RInside &rInside			= rinside->instance();

	rInside[".numDecimals"]			= numDecimals;
	rInside[".fixedDecimals"]		= fixedDecimals;
	rInside[".normalizedNotation"]	= normalizedNotation;
	rInside[".exactPValues"]		= exactPValues;
}

void STDCALL jaspRCPP_setFontAndPlotSettings(const char * resultFont, const int ppi, const char* imageBackground)
{
	RInside &rInside			= rinside->instance();

	rInside[".resultFont"]			= resultFont;
	rInside[".imageBackground"]		= imageBackground;
	rInside[".ppi"]					= ppi;

    // sometimes jaspBase is not available
    jaspRCPP_parseEvalQNT("try(jaspBase:::registerFonts())");
}

const char* STDCALL jaspRCPP_runModuleCall(const char* name, const char* title, const char* moduleCall, const char* dataKey, const char* options,
										   const char* stateKey, int analysisID, int analysisRevision, bool developerMode)
{
	RInside &rInside			= rinside->instance();

	rInside["name"]					= name;
	rInside["title"]				= title;
	rInside["options"]				= options;
	rInside["dataKey"]				= dataKey;
	rInside["stateKey"]				= stateKey;
	rInside["moduleCall"]			= moduleCall;
	rInside["resultsMeta"]			= "null";
	rInside["requiresInit"]			= false;
	
	_setJaspResultsInfo(analysisID, analysisRevision, developerMode);

	static std::string str;
	try
	{
		SEXP results = jaspRCPP_parseEval("jaspBase::runJaspResults(name=name, title=title, dataKey=dataKey, options=options, stateKey=stateKey, functionCall=moduleCall)", true);

		if(results != NULL && Rcpp::is<std::string>(results))	str = Rcpp::as<Rcpp::String>(results);
		else													str = "error!";
	}
	catch(std::runtime_error & e)
	{
		rInside["errorJaspResultsCrash"] = e.what();
		jaspRCPP_parseEvalQNT("jaspBase:::sendFatalErrorMessage(name=name, title=title, msg=errorJaspResultsCrash);");
	}

#ifdef PRINT_ENGINE_MESSAGES
	jaspRCPP_logString("result of runJaspResults:\n" + str + "\n");
#endif

	jaspRCPP_parseEvalQNT("jaspBase:::destroyAllAllocatedObjects()");

	jaspRCPP_checkForCrashRequest();

	return str.c_str();
}

void STDCALL jaspRCPP_runScript(const char * scriptCode)
{
	jaspRCPP_parseEvalQNT(scriptCode);

	jaspRCPP_checkForCrashRequest();

	return;
}

const char * STDCALL jaspRCPP_runScriptReturnString(const char * scriptCode)
{
	static std::string returnStr;
	std::string script(scriptCode);
	returnStr = jaspRCPP_parseEvalStringReturn(script);

	jaspRCPP_checkForCrashRequest();

	return returnStr.c_str();
}

int STDCALL jaspRCPP_runFilter(const char * filterCode, bool ** arrayPointer)
{
	jaspRCPP_logString("jaspRCPP_runFilter runs: \n\"" + std::string(filterCode) + "\"\n" );

	jaspRCPP_resetErrorMsg();
	rinside->instance()[".filterCode"] = filterCode;

	const std::string filterTryCatch(
		"returnVal = 'null'; \n"
		"print(paste0('Running filtercode: ', .filterCode)); \n"
		"tryCatch(\n"
		"	{ returnVal <- eval(parse(text=.filterCode)) }, \n"
		"	warning	= function(w) { .setRWarning(toString(w$message))	}, \n"
		"	error	= function(e) { .setRError(  toString(e$message))	}  \n"
		"); \n"
		"returnVal");

	SEXP result = jaspRCPP_parseEval(filterTryCatch);

	jaspRCPP_checkForCrashRequest();

	if(Rcpp::is<Rcpp::NumericVector>(result) || Rcpp::is<Rcpp::LogicalVector>(result))
	{
		Rcpp::NumericVector vec(result);

		if(vec.size() == 0)
			return 0;

		(*arrayPointer) = (bool*)malloc(vec.size() * sizeof(bool));

		for(int i=0; i<vec.size(); i++)
			(*arrayPointer)[i] = vec[i] == 1;

		return vec.size();
	}

	return -1;
}

void STDCALL jaspRCPP_resetErrorMsg()
{
	lastErrorMessage = "";
}

void STDCALL jaspRCPP_setErrorMsg(const char* msg)
{
	lastErrorMessage = msg;
}

const char*	STDCALL jaspRCPP_getLastErrorMsg()
{
	return lastErrorMessage.c_str();
}

void STDCALL jaspRCPP_freeArrayPointer(bool ** arrayPointer)
{
	free(*arrayPointer);
}

const char* STDCALL jaspRCPP_saveImage(const char * data, const char *type, const int height, const int width)
{
	RInside &rInside = rinside->instance();

	rInside["plotName"]				= data;
	rInside["format"]				= type;
	rInside["height"]				= height;
	rInside["width"]				= width;

	static std::string staticResult;
	staticResult = jaspRCPP_parseEvalStringReturn("jaspBase:::saveImage(plotName, format, height, width)", true);
	return staticResult.c_str();
}

const char* STDCALL jaspRCPP_editImage(const char * name, const char * optionsJson, int analysisID)
{

	RInside &rInside = rinside->instance();

	rInside[".editImgOptions"]		= optionsJson;
	rInside[".analysisName"]		= name;

	_setJaspResultsInfo(analysisID, 0, false);

	static std::string staticResult;
	staticResult =  jaspRCPP_parseEvalStringReturn("jaspBase:::editImage(.analysisName, .editImgOptions)", true);

	return staticResult.c_str();
}


void STDCALL jaspRCPP_rewriteImages(const char * name, int analysisID)
{

	RInside &rInside = rinside->instance();

	rInside[".analysisName"]		= name;

	_setJaspResultsInfo(analysisID, 0, false);

	jaspRCPP_parseEvalQNT("jaspBase:::rewriteImages(.analysisName, .ppi, .imageBackground)", true);
}



const char*	STDCALL jaspRCPP_evalComputedColumn(const char *rCode, const char * setColumnCode) 
{
	// Function to evaluate computed column R code from C++
	// Returns string if R result is a string, else returns "null"
	// Can also load the entire dataset if need be
	bool setWd = false;

	//jaspRCPP_logString(std::string("jaspRCPP_evalComputedColumn runs: \n\"") + rCode + "\"\nand \""+setColumnCode+"\"\n" );

	lastErrorMessage = "";
	rinside->instance()[".rCode"] = rCode;
	const std::string rCodeTryCatch(""
		"returnVal = NULL;	"
		"tryCatch("
		"    suppressWarnings({	returnVal <- eval(parse(text=.rCode))     }),	"
		"    error	= function(e) { .setRError( toString(e$message)) } 	"
		")"
		"; returnVal	");

	static std::string staticResult;
	try
	{
		try
		{
			rinside->instance()[".calcedVals"]	= Rcpp::RObject(jaspRCPP_parseEval(rCodeTryCatch,	false, false));
		}
		catch(std::runtime_error e)
		{
			jaspRCPP_setErrorMsg(e.what());	
			staticResult						=	NullString;
			rinside->instance()[".calcedVals"]	=	NULL;
		}
		
		staticResult = jaspRCPP_parseEvalStringReturn(setColumnCode,	false, false);
		
		rinside->instance()[".calcedVals"]	=	NULL;
		
	}
	catch(...)
	{
		staticResult = NullString;
	}

	return staticResult.c_str();
}

const char*	STDCALL jaspRCPP_evalRCode(const char *rCode, bool setWd) {
	// Function to evaluate arbitrary R code from C++
	// Returns string if R result is a string, else returns "null"
	// Can also load the entire dataset if need be


	jaspRCPP_logString(std::string("jaspRCPP_evalRCode runs: \n\"") + rCode + "\"\n" );

	lastErrorMessage = "";
	rinside->instance()[".rCode"] = rCode;
	const std::string rCodeTryCatch(""
		"returnVal = 'null';	"
		"tryCatch("
		"    suppressWarnings({	returnVal <- eval(parse(text=.rCode))     }),	"
		"    error	= function(e) { .setRError(  paste0(toString(e$message), '\n', paste0(sys.calls()[sys.nframe():2], collapse='\n'))) } 	"
		")"
		"; returnVal	");

	static std::string staticResult;
	try
	{
		staticResult = jaspRCPP_parseEvalStringReturn(rCodeTryCatch, setWd);
	}
	catch(...)
	{
		staticResult = NullString;
	}

	return staticResult.c_str();
}

std::stringstream __cmderLogStream;

int __cmderLogFlush() { __cmderLogStream.flush(); return 0; }

size_t __cmderLogWrite(const void * buf, size_t len)
{
	try {	if(len > 0)	__cmderLogStream.write(static_cast<const char *>(buf), len);	}
	catch (...) {		__cmderLogStream << "Capturing output from R had a problem...\n" << std::flush; }
	return len;
}

///Run Rcode and return all output as if
const char*	STDCALL jaspRCPP_evalRCodeCommander(const char *rCode)
{
	__cmderLogStream.str("");

	logFlushDef originalFlush	= _logFlushFunction;
	_logFlushFunction			= __cmderLogFlush;

	logWriteDef	originalLogger	= _logWriteFunction;
	_logWriteFunction			= __cmderLogWrite;

	lastErrorMessage = "";
	rinside->instance()[".rCode"] = rCode;
	const std::string rCodeTryCatch(""
		"withCallingHandlers("															"\n"
		"  { "																			"\n"
		"    options(warn=1);"															"\n"
		"    valVis <- withVisible(eval(parse(text=.rCode)));"							"\n"
		"    if(valVis$visible) print(valVis$value);"									"\n"
		"  },	"																		"\n"
		"  warning = function(w) { cat(paste0('Warning: ', toString(w$message))) },"	"\n"
		"  error   = function(e) { cat(paste0('Error: ',   toString(e$message))) }"		"\n"
		");"
		);

	jaspRCPP_parseEvalQNT(rCodeTryCatch, false, false);

	_logFlushFunction			= originalFlush;
	_logWriteFunction			= originalLogger;

	static std::string staticLog;

	staticLog = __cmderLogStream.str();

	__cmderLogStream.str("");

	return staticLog.c_str();
}

} // extern "C"

SEXP jaspRCPP_requestTempFileNameSEXP(SEXP extension)
{
	const char *root, *relativePath;
	std::string extensionAsString = Rcpp::as<std::string>(extension);

	if (!requestTempFileNameCB(extensionAsString.c_str(), &root, &relativePath))
		return R_NilValue;

	Rcpp::List paths;
	paths["root"]			= root;
	paths["relativePath"]	= relativePath;

	return paths;
}

SEXP jaspRCPP_requestSpecificFileNameSEXP(SEXP filename)
{
	const char *root, *relativePath;
	std::string filenameAsString = Rcpp::as<std::string>(filename);

	if (!requestSpecificFileNameCB(filenameAsString.c_str(), &root, &relativePath))
		return R_NilValue;

	Rcpp::List paths;
	paths["root"]			= root;
	paths["relativePath"]	= relativePath;

	return paths;
}

SEXP jaspRCPP_requestTempRootNameSEXP()
{
	const char* root = requestTempRootNameCB();

	Rcpp::List paths;
	paths["root"] = root;
	return paths;
}

SEXP jaspRCPP_allColumnNamesDataset()
{
	size_t			cols;
	const char **	names = getAllColumnNames(cols, true);
	
	Rcpp::StringVector colNames;
	
	for(size_t i=0; i<cols; i++)
		colNames.push_back(names[i]);
	
	return colNames;
}


bool jaspRCPP_requestJaspResultsRelativeFilePath(std::string & root, std:: string & relativePath)
{
	root		 = "";
	relativePath = "";

	const char	* _root,
				* _relativePath;

	if (!requestJaspResultsFileSourceCB(&_root, &_relativePath))
		return false;

	root			= _root;
	relativePath	= _relativePath;

	return true;
}

bool jaspRCPP_requestSpecificRelativeFilePath(std::string specificFilename, std::string & root, std:: string & relativePath)
{
	root		 = "";
	relativePath = "";

	const char	* _root,
				* _relativePath;

	if (!requestSpecificFileNameCB(specificFilename.c_str(), &_root, &_relativePath))
		return false;

	root			= _root;
	relativePath	= _relativePath;

	return true;
}

SEXP jaspRCPP_requestStateFileNameSEXP()
{
	const char* root;
	const char* relativePath;

	if (!requestStateFileSourceCB(&root, &relativePath))
		return R_NilValue;

	Rcpp::List paths;
	paths["root"]			= root;
	paths["relativePath"]	= relativePath;

	return paths;
}


SEXP jaspRCPP_callbackSEXP(SEXP in, SEXP progress)
{
	std::string inStr	= Rf_isNull(in)			? "null"	: Rcpp::as<std::string>(in);
	int progressInt		= Rf_isNull(progress)	? -1		: Rcpp::as<int>(progress);
	const char *out;

	return runCallbackCB(inStr.c_str(), progressInt, &out) ? Rcpp::CharacterVector(out) : 0;
}

void jaspRCPP_returnDataFrame(Rcpp::DataFrame frame)
{
	long colcount = frame.size();

	std::cout << "got a dataframe!\n" << colcount << "X" << (colcount > 0 ? Rcpp::as<Rcpp::NumericVector>(frame[0]).size() : -1) << "\n" << std::flush;

	if(colcount > 0)
	{
		long rowcount = Rcpp::as<Rcpp::NumericVector>(frame[0]).size();

		for(long row=0; row<rowcount; row++)
		{
			for(long col=0; col<colcount; col++)
				std::cout << "'" << Rcpp::as<Rcpp::StringVector>(frame[col])[row] << " or " <<  Rcpp::as<Rcpp::NumericVector>(frame[col])[row]  << "'\t" << std::flush;

			std::cout << "\n";
		}
		std::cout << std::flush;
	}
}

void jaspRCPP_returnString(SEXP Message)
{
	jaspRCPP_logString("A message from R: " + Rcpp::as<std::string>(Message) + "\n");
}

void jaspRCPP_setRWarning(SEXP Message)
{
	lastErrorMessage = Rcpp::as<std::string>(Message);
}

void jaspRCPP_setRError(SEXP Message)
{
	lastErrorMessage = Rcpp::as<std::string>(Message);
}

void jaspRCPP_setLog(SEXP Message)
{
	lastErrorMessage = Rcpp::as<std::string>(Message);
}

int jaspRCPP_dataSetRowCount()
{
	return dataSetRowCount();
}

columnType jaspRCPP_getColumnType(std::string columnName)
{
	return columnType(dataSetGetColumnType(columnName.c_str())); // columnName decoded in rbridge
}

int jaspRCPP_getColumnAnalysisId(std::string columnName)
{
	return dataSetGetColumnAnalysisId(columnName.c_str()); // columnName decoded in rbridge
}

std::string jaspRCPP_createColumn(std::string columnName)
{
	return dataSetCreateColumn(columnName.c_str());
}

bool jaspRCPP_deleteColumn(std::string columnName)
{
	return dataSetDeleteColumn(columnName.c_str());
}


bool jaspRCPP_getColumnExists(std::string columnName)
{
	
	size_t			cols;
	const char	**	decoded	= getAllColumnNames(cols, false);
	
	for(size_t i=0; i<cols; i++)
		if(decoded[i] == columnName)
			return true;
	
	//Maybe the R author already encoded the name?
	const char	**	encoded = getAllColumnNames(cols, true); // This also changes `decoded` above, and in fact, `decoded == encoded` probably
	for(size_t i=0; i<cols; i++)
		if(encoded[i] == columnName)
			return true;
	
	return false;
}

bool jaspRCPP_columnIsScale(		const std::string & columnName) { return jaspRCPP_getColumnType(columnName) == columnType::scale;		}
bool jaspRCPP_columnIsOrdinal(		const std::string & columnName) { return jaspRCPP_getColumnType(columnName) == columnType::ordinal;		}
bool jaspRCPP_columnIsNominal(		const std::string & columnName) { return jaspRCPP_getColumnType(columnName) == columnType::nominal;		}

bool jaspRCPP_setColumnDataAsScale(const std::string & columnName, Rcpp::RObject scalarData)
{
	return _jaspRCPP_setColumnDataAndType(columnName, scalarData, columnType::scale);
}


bool jaspRCPP_setColumnDataAsOrdinal(const std::string & columnName, Rcpp::RObject ordinalData)
{
	return _jaspRCPP_setColumnDataAndType(columnName, ordinalData, columnType::ordinal);
}


bool jaspRCPP_setColumnDataAsNominal(const std::string & columnName, Rcpp::RObject nominalData)
{
	return _jaspRCPP_setColumnDataAndType(columnName, nominalData, columnType::nominal);
}

bool _jaspRCPP_setColumnDataAndType(const std::string & columnName, Rcpp::RObject data, columnType colType)
{
	static Rcpp::Function asNumeric("as.numeric");
	Rcpp::Vector<STRSXP>	strData = Rf_isNull(data) ? Rcpp::Vector<STRSXP>()	: Rcpp::as<Rcpp::Vector<STRSXP>>(data);
	Rcpp::Vector<REALSXP>	dblData = Rf_isNull(data) ? Rcpp::NumericVector()	: Rcpp::NumericVector(asNumeric(Rcpp::_["x"] = data));
	
	std::vector<std::string> convertedStrings(strData.begin(), strData.end());

	const char ** nominals = new const char*[convertedStrings.size()]();

	for(size_t i=0; i<convertedStrings.size(); i++)
		nominals[i] = std::isnan(dblData[i]) && convertedStrings[i] == "NA" ? "" : convertedStrings[i].c_str();

	return dataSetColumnDataAndType(columnName.c_str(), nominals, static_cast<size_t>(strData.size()), int(colType));
}


void jaspRCPP_setColumnDataHelper_FactorsLevels(Rcpp::Vector<INTSXP> data, int *& outputData, size_t & numLevels, const char **& labelPointers, std::string *& labels)
{
	Rcpp::CharacterVector	levels;
	numLevels = 0;

	if(!Rf_isNull(data.attr("levels")))
	{
		levels = data.attr("levels");
		numLevels = size_t(levels.size());
	}

	if(numLevels > 0)
	{
		labels			= new std::string [numLevels];
		labelPointers	= new const char * [numLevels];

		for(int i=0; i<numLevels; i++)
		{
			labels[i]			= levels[i];
			labelPointers[i]	= labels[i].c_str();
		}
	}
	else
	{
		std::set<int> unique;
		for(int i=0; i<data.size(); i++)
			unique.insert(outputData[i]);
		numLevels = unique.size();

		std::vector<int> sorted(unique.begin(), unique.end());
		std::sort(sorted.begin(), sorted.end());

		labels			= new std::string [numLevels];
		labelPointers	= new const char * [numLevels];

		std::map<std::string, int> levelToVal;

		for(size_t i=0; i<sorted.size(); i++)
		{
			labels[i]				= std::to_string(sorted[i]);
			labelPointers[i]		= labels[i].c_str();
			levelToVal[labels[i]]	= i + 1;
		}

		for(int i=0; i<data.size(); i++)
			outputData[i] = levelToVal[std::to_string(outputData[i])];;
	}
}


RBridgeColumnType* jaspRCPP_marshallSEXPs(SEXP columns, SEXP columnsAsNumeric, SEXP columnsAsOrdinal, SEXP columnsAsNominal, SEXP allColumns, size_t * colMax)
{
	std::map<std::string, columnType>	columnsRequested;
	std::map<std::string, size_t>		columnsOrder;		//Let's remember the order in which they are requested

	if (Rf_isLogical(allColumns) && Rcpp::as<bool>(allColumns))
	{
		char** columns = readDataColumnNamesCB(colMax);

		if (columns)
		{
			for (size_t i = 0; i < *colMax; i++)
				columnsRequested[columns[i]] = columnType::unknown;
		}
	}

	*colMax = 0;

	auto setTypeRequested = [&] (SEXP cols, columnType SetThis)
	{
		if(Rf_isString(cols))
		{
			std::vector<std::string> tmps = Rcpp::as<std::vector<std::string>>(cols);
			for (const std::string & tmp : tmps)
				if(columnsOrder.count(tmp) == 0)
				{
					columnsRequested[tmp]	= SetThis;
					columnsOrder[tmp]		= (*colMax)++;
				}

				else if(columnsOrder.count(tmp) > 0 && columnsRequested[tmp] == columnType::unknown)
					columnsRequested[tmp] = SetThis; //If type is unknown then we simply overwrite it with a manually specified type of analysis

				else if( !(columnsOrder.count(tmp) > 0 && columnsRequested[tmp] == SetThis) ) //Only give an error if the type is different from what is requested
                    Rf_error("You've specified column '%s' for more than one columntype!!!\nNo clue which one we should give back...", tmp.c_str());

		}
	};

	setTypeRequested(columns,			columnType::unknown);
	setTypeRequested(columnsAsNumeric,	columnType::scale);
	setTypeRequested(columnsAsOrdinal,	columnType::ordinal);
	setTypeRequested(columnsAsNominal,	columnType::nominal);

	size_t lastOrderedIndex = *colMax;

	*colMax = columnsRequested.size();

	RBridgeColumnType* result = static_cast<RBridgeColumnType*>(calloc(*colMax, sizeof(RBridgeColumnType)));

	for (auto const &columnRequested : columnsRequested)
	{
		size_t colNo		= columnsOrder.count(columnRequested.first) > 0 ? columnsOrder[columnRequested.first] : lastOrderedIndex++;
		result[colNo].name	= strdup(columnRequested.first.c_str());
		result[colNo].type	= int(columnRequested.second);
	}

	return result;
}

void freeRBridgeColumnType(RBridgeColumnType *columns, size_t colMax)
{
	for (int i = 0; i < colMax; i++)
		free(columns[i].name);

	free(columns);
}

Rcpp::DataFrame jaspRCPP_readFullDataSet()
{
	size_t			colMax		= 0;
	RBridgeColumn * colResults	= readFullDataSetCB(&colMax);
	
	return jaspRCPP_convertRBridgeColumns_to_DataFrame(colResults, colMax);
}


Rcpp::DataFrame jaspRCPP_readFullFilteredDataSet()
{
	size_t			colMax		= 0;
	RBridgeColumn * colResults	= readFullFilteredDataSetCB(&colMax);
	
	return jaspRCPP_convertRBridgeColumns_to_DataFrame(colResults, colMax);
}

Rcpp::DataFrame jaspRCPP_readFilterDataSet()
{
	size_t			colMax		= 0;
	RBridgeColumn * colResults	= readFilterDataSetCB(&colMax);

	if(colMax == 0)
		return Rcpp::DataFrame();

	return jaspRCPP_convertRBridgeColumns_to_DataFrame(colResults, colMax);
}

Rcpp::DataFrame jaspRCPP_readDataSetSEXP(SEXP columns, SEXP columnsAsNumeric, SEXP columnsAsOrdinal, SEXP columnsAsNominal, SEXP allColumns)
{
	size_t				colMax				= 0;
	RBridgeColumnType * columnsRequested	= jaspRCPP_marshallSEXPs(columns, columnsAsNumeric, columnsAsOrdinal, columnsAsNominal, allColumns, &colMax);
	RBridgeColumn	  * colResults			= readDataSetCB(columnsRequested, colMax, true);
	
	freeRBridgeColumnType(columnsRequested, colMax);

	return jaspRCPP_convertRBridgeColumns_to_DataFrame(colResults, colMax);
}

Rcpp::DataFrame jaspRCPP_convertRBridgeColumns_to_DataFrame(const RBridgeColumn* colResults, size_t colMax)
{
	Rcpp::DataFrame dataFrame = Rcpp::DataFrame();

	if (colResults)
	{
		Rcpp::List			list(colMax);
		Rcpp::StringVector	columnNames(colMax);

		for (int i = 0; i < int(colMax); i++)
		{
			const RBridgeColumn &	colResult	= colResults[i];

			columnNames[i] = colResult.name;

			if (colResult.isScale)			list[i] =						Rcpp::NumericVector(colResult.doubles,	colResult.doubles	+ colResult.nbRows);
			else							list[i] = jaspRCPP_makeFactor(	Rcpp::IntegerVector(colResult.ints,		colResult.ints		+ colResult.nbRows), colResult.labels, colResult.nbLabels, colResult.isOrdinal);

		}

		list.attr("names")			= columnNames;
		dataFrame					= Rcpp::DataFrame(list);
		dataFrame.attr("row.names") = Rcpp::IntegerVector(colResults[colMax].ints, colResults[colMax].ints + colResults[colMax].nbRows);
	}

	return dataFrame;
}

Rcpp::DataFrame jaspRCPP_readDataSetHeaderSEXP(SEXP columns, SEXP columnsAsNumeric, SEXP columnsAsOrdinal, SEXP columnsAsNominal, SEXP allColumns)
{
	size_t colMax = 0;
	RBridgeColumnType* columnsRequested				= jaspRCPP_marshallSEXPs(columns, columnsAsNumeric, columnsAsOrdinal, columnsAsNominal, allColumns, &colMax);
	RBridgeColumnDescription* columnsDescription	= readDataSetDescriptionCB(columnsRequested, colMax);

	freeRBridgeColumnType(columnsRequested, colMax);

	Rcpp::DataFrame dataFrame = Rcpp::DataFrame();

	if (columnsDescription)
	{
		Rcpp::List list(colMax);
		Rcpp::StringVector columnNames(colMax);

		for (size_t i = 0; i < colMax; i++)
		{
			RBridgeColumnDescription& colDescription = columnsDescription[i];

			columnNames[i] = colDescription.name;

			if (colDescription.isScale)				list(i) = Rcpp::NumericVector(0);
			else									list(i) = jaspRCPP_makeFactor(Rcpp::IntegerVector(0), colDescription.labels, colDescription.nbLabels, colDescription.isOrdinal);
		}

		list.attr("names") = columnNames;
		dataFrame = Rcpp::DataFrame(list);
	}

	return dataFrame;

}

Rcpp::IntegerVector jaspRCPP_makeFactor(Rcpp::IntegerVector v, char** levels, int nbLevels, bool ordinal)
{
/*#ifdef JASP_DEBUG
	std::cout << "jaspRCPP_makeFactor:\n\tlevels:\n\t\tnum: " << nbLevels << "\n\t\tstrs:\n";
	for(int i=0; i<nbLevels; i++)
		std::cout << "\t\t\t'" << levels[i] << "'\n";
	std::cout << "intVec: ";

	for(int i=0; i<v.size(); i++)
		std::cout << v[i] << (i < v.size() - 1 ? ", " : "" );
	std::cout << std::endl;
#endif*/

	Rcpp::CharacterVector labels(nbLevels);
	for (int i = 0; i < nbLevels; i++)
		labels[i] = levels[i];

	v.attr("levels") = labels;

	std::vector<std::string> rClass;

	if (ordinal) rClass.push_back("ordered");
	rClass.push_back("factor");

	v.attr("class") = rClass;

	if(v.size() == 0)
		return v;

	static Rcpp::Function droplevels("droplevels");
	return droplevels(Rcpp::_["x"] = v);
}

void jaspRCPP_crashPlease() { shouldCrashSoon = true; }
void jaspRCPP_checkForCrashRequest()
{
	if(shouldCrashSoon)
		throw std::runtime_error("User requested a crash");
}

struct jaspRCPP_Connection
{
	static Rboolean	open(struct Rconn *)		{ return Rboolean::TRUE;	}
	static void		close(struct Rconn *)		{}
	static void		destroy(struct Rconn *)		{}
	static int		fflush(struct Rconn *)		{ return 0;	}

	static size_t	write(const void * buf, size_t, size_t len, struct Rconn * = nullptr)
	{
		return _logWriteFunction(buf, len);
	}

	static int vfprintf(struct Rconn *, const char * format, va_list args)
	{
		const int maxChar = 1024 * 1024 * 30; //30MB should be enough for any crazy stuff right?
		static std::vector<char> buf(maxChar);

		int l = std::vsnprintf(buf.data(), maxChar, format, args);

		write(buf.data(), 0, l);

		return l;
	}
};


void jaspRCPP_logString(const std::string & logThis)
{
	jaspRCPP_Connection::write(logThis.c_str(), 0, logThis.size(), nullptr);
}

void jaspRCPP_parseEvalPreface(const std::string & code, const char * msg = "Evaluating R-code:\n")
{
	jaspRCPP_logString(msg);
	jaspRCPP_Connection::write(code.c_str(), 0, code.size(), nullptr);
	jaspRCPP_logString("\nOutput:\n");
}

std::string __sinkMe(const std::string code)
{
	return	"sink(.outputSink);\n" + code; //default type = c('message', 'output') anyway
}

void jaspRCPP_setWorkingDirectory()
{
	std::string root = requestTempRootNameCB();
	std::string code = "setwd(\"" + root + "\");";
	rinside->parseEvalQNT(__sinkMe(code));
	rinside->parseEvalQNT("sink();"); //Back to normal!
}

void jaspRCPP_parseEvalQNT(const std::string & code, bool setWd, bool preface)
{
	if(setWd)
		jaspRCPP_setWorkingDirectory();

	if(preface)	
		jaspRCPP_parseEvalPreface(code);
	
	rinside->parseEvalQNT(__sinkMe());
	rinside->parseEvalQNT(code);
	jaspRCPP_logString("\n");
	rinside->parseEvalQNT("sink();"); //Back to normal!
}

std::string jaspRCPP_parseEvalStringReturn(const std::string & code, bool setWd, bool preface)
{
	RInside::Proxy res = jaspRCPP_parseEval(code, setWd, preface);
	
	return Rf_isString(res) ? Rcpp::as<std::string>(res) : NullString;
}


RInside::Proxy jaspRCPP_parseEval(const std::string & code, bool setWd, bool preface)
{
	if (setWd)
		jaspRCPP_setWorkingDirectory();

	if(preface)	
		jaspRCPP_parseEvalPreface(code);
	
	rinside->parseEvalQNT(__sinkMe());
	RInside::Proxy returnthis = rinside->parseEval(code); //Not throwing is nice actually! Well, unless you want to hear about missing modules etc...
	jaspRCPP_logString("\n");

	rinside->parseEvalQNT("sink();"); //back to normal!

	return returnthis;
}

std::string _jaspRCPP_System(std::string cmd)
{
	return _systemFunc(cmd.c_str());
}

///This function runs *code* in a separate instance of R, because the output of install.packages (and perhaps other functions) cannot be captured through the outputsink...
SEXP jaspRCPP_RunSeparateR(SEXP code)
{
	auto bendSlashes = [](std::string input)
	{
#ifdef WIN32
		std::stringstream output;

		for(char k : input)
			if(k == '/')	output << "\\";
			else			output << k;
		return output.str();
#else
		return input;
#endif
	};

	static std::string R = bendSlashes("\""+ _R_HOME + "/bin/R\"");

	std::string codestr = Rcpp::as<std::string>(code),
				command = R + " --slave -e \"" + codestr + "\"";

	std::string out = _jaspRCPP_System(command);

	jaspRCPP_logString(out + "\n");

	return Rcpp::wrap(out);
}

// see https://gcc.gnu.org/onlinedocs/gcc-4.8.5/cpp/Stringification.html
#define xstr(s) str(s)
#define str(s)  #s

void jaspRCPP_postProcessLocalPackageInstall(SEXP moduleLibrary)
{
	if(Rf_isString(moduleLibrary))
		_libraryFixerFunc(Rcpp::as<std::string>(moduleLibrary).c_str());
	else
		Rf_error("jaspRCPP_postProcessLocalPackageInstall did not receive a string, it should get that and the string should represent some kind of R library path.");
}

Rcpp::String jaspRCPP_encodeColumnNameRcpp(const Rcpp::String & in)
{
	return encodeColumnName(std::string(in).c_str());
}

Rcpp::String jaspRCPP_decodeColumnNameRcpp(const Rcpp::String & in)
{
	return decodeColumnName(std::string(in).c_str());
}

Rcpp::String jaspRCPP_encodeAllColumnNames(const Rcpp::String & in)
{
	return encodeAllColumnNames(std::string(in).c_str());
}

Rcpp::String jaspRCPP_decodeAllColumnNames(const Rcpp::String & in)
{
	return decodeAllColumnNames(std::string(in).c_str());
}

bool jaspRCPP_shouldEncodeColumnName(std::string in)
{
	return shouldEncodeColumnName(in.c_str());
}

bool jaspRCPP_shouldDecodeColumnName(std::string in)
{
	return shouldDecodeColumnName(in.c_str());
}

std::string jaspRCPP_encodeColumnName(std::string in)
{
	return encodeColumnName(in.c_str());
}

std::string jaspRCPP_decodeColumnName(std::string in)
{
	return decodeColumnName(in.c_str());
}

// ------------------- Below here be dragons -------------------- //

extern "C" {
//We need to do the following crazy defines to make sure the header actually gets accepted by the compiler...
#define class _class
#define private _private;
#include "R_ext/Connections.h"
}

SEXP jaspRCPP_CreateCaptureConnection()
{
	Rconnection con;

	SEXP rc = PROTECT(R_new_custom_connection("jaspRCPP_OUT", "w", "jaspRCPP_OUT", &con));

	con->incomplete		= FALSE;
	con->canseek		= FALSE;
	con->canwrite		= TRUE;
	con->isopen			= TRUE;
	con->blocking		= TRUE;
	con->text			= TRUE;
	con->UTF8out		= TRUE;
	con->open			= jaspRCPP_Connection::open;
	con->close			= jaspRCPP_Connection::close;
	con->destroy		= jaspRCPP_Connection::destroy;
	con->fflush			= jaspRCPP_Connection::fflush;
	con->write			= jaspRCPP_Connection::write;
	con->vfprintf		= jaspRCPP_Connection::vfprintf;

	UNPROTECT(1);
	return rc;
}
