<% @Language='JScript' %>

<%
    // This is the Microsoft NT4 version of RedirectorPASP.asp.

    var requestPath = Request.ServerVariables("HTTP_SSXSLSRCFILE:");
    var preprocessor = Application("ASPPreprocessor");
    var processedFile;
    var errDescrip = null;
    var errCode = null;

    if (preprocessor != null) {
        try {
            var queryString = Request.ServerVariables("Query_String");

            processedFile = preprocessor.Process(requestPath);
            if (queryString != "") {
                processedFile = processedFile + "?" + queryString;
            }
            Response.Redirect(processedFile);
        } catch (exception) {
            errDescrip = "Resource not found";
            errCode = "404 Not Found";
        }

    } else {

        errDescrip = "The 'ASPPreprocesser' application object is undefined.  It needs to be initialized in the application's global.asa file.";
        errCode = "500.100 Internal Server Error - ASP Error";
    }

    if (errDescrip) {
        var servDoc =
            new ActiveXObject("XSLISAPI.XMLServerDocument");
        servDoc.SetError(errDescrip,
                         requestPath,
                         errCode);
        servDoc.HandleError(Response);
    }
        
%>
