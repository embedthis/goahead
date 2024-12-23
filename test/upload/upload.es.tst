/*
    upload.tst - File upload tests
 */

require ejs.unix

const HTTP = tget('TM_HTTP') || '127.0.0.1:8080'
let http: Http = new Http

if (thas('ME_GOAHEAD_UPLOAD')) {

    //  Upload a small file
    http.upload(HTTP + '/action/uploadTest', { myfile: 'small.dat'} )
    ttrue(http.status == 200)
    ttrue(http.response.contains('CLIENT=small.dat'))
    ttrue(http.response.contains('SIZE=29'))
    ttrue(http.response.contains('FILENAME='))
    ttrue(http.response.contains('FILE_FILENAME_myfile='))
    ttrue(http.response.contains('FILE_CLIENT_FILENAME_myfile=small.dat'))
    ttrue(http.response.contains('FILE_SIZE_myfile=29'))
    http.wait()

    //  Test with form data
    http.upload(HTTP + '/action/uploadTest', { myfile: 'small.dat'}, {name: 'John Smith', address: '100 Mayfair'} )
    ttrue(http.status == 200)
    ttrue(http.response.contains('CLIENT=small.dat'))
    ttrue(http.response.contains('SIZE=29'))
    ttrue(http.response.contains('FILENAME='))
    ttrue(http.response.contains('FILE_FILENAME_myfile='))
    ttrue(http.response.contains('FILE_CLIENT_FILENAME_myfile=small.dat'))
    ttrue(http.response.contains('FILE_SIZE_myfile=29'))
    ttrue(http.response.contains('name=John Smith'))
    ttrue(http.response.contains('address=100 Mayfair'))

} else {
    tskip('Upload support not enabled')
}
