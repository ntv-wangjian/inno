/**
 * (C) Roger Hardiman <opensource@rjh.org.uk>
 * First Release - May 2018
 * Licenced with the MIT Licence
 *
 * Perform a brute force scan of the network looking for ONVIF devices
 * For each device, save the make and model and a snapshot in the audit folder
 *
 * Can also use ONVIF Discovery to trigger the scan
 Modified by WJ on 2019-01-02
 探测过程是个异步操作
 */

var IPADDRESS = '192.168.1.1-192.168.1.254', // single address or a range
    PORT = 80,
    USERNAME = 'onvifusername',
    PASSWORD = 'onvifpassword';

var onvif = require('onvif');
var Cam = onvif.Cam;
var flow = require('nimble');
var http = require('http');
var https= require('https');
var args = require('commander');
var fs = require('fs');
var dateTime = require('node-datetime');
var path = require('path');
var parseString = require('xml2js').parseString;
var stripPrefix = require('xml2js').processors.stripPrefix;

var url = require('url');

// Show Version
var version = require('./package.json').version;
args.version(version);
args.description('ONVIF DEVICE PROBER(copyright 2018, http://www.ruiboyun.com)');
args.option('-f, --filename <value>', 'Filename of JSON file with IP Address List');
args.option('-i, --ipaddress <value>', 'IP Address (x.x.x.x) or IP Address Range (x.x.x.x-y.y.y.y)');
args.option('-P, --port <value>', 'ONVIF Port. Default 80');
args.option('-u, --username <value>', 'ONVIF Username');
args.option('-p, --password <value>', 'ONVIF Password');
args.option('-s, --scan', 'Discover Network devices on local subnet');
args.option('-t, --timeout <value>', 'Probe Timeout,seconds');
args.option('-r, --request <value>', 'Where to report');
args.option('-o, --output <value>', 'Where to save report');

let copyright = "NOVEL-TV ONVIF DEVICE PROBER @2019 云视睿博  http://www.ruiboyun.com";
console.log(copyright);
if(copyright.indexOf('ruiboyun')<0){
	return;
}

args.parse(process.argv);

if (!args) {
    args.help();
    process.exit(1);

}

if (!args.filename && !args.ipaddress && !args.scan) {
    console.log('Requires either a Filename (-f) or an IP Address/IP Range (-i) or a Scan (-s)');
    console.log('Use -h for details');
    process.exit(1);
}

console.log("NOVEL-TV ONVIF DEVICE PROBER START...");

let time_now = dateTime.create();
//let folder = 'onvif_audit_report_' + time_now.format('Y_m_d_H_M_S');
let folder = 'onvif';

//WJ
var gtimeout = 2000;
var postUrl  = null;
if(args.timeout){
	gtimeout = parseInt(args.timeout);
	if(gtimeout<200 || gtimeout>10000){
		gtimeout = 2000;
	}
}
if(args.request){
	postUrl = args.request;
}
if(args.output){
	folder = args.output;
}


let folder_report = folder + path.sep + "report";
let folder_image  = folder + path.sep + "snapshot";

try {
	if(!fs.existsSync(folder)){
		fs.mkdirSync(folder);
	}
    if(fs.existsSync(folder_report)){
    	//clearFiles(folder_report);
    }else{
	    fs.mkdirSync(folder_report);
	}
    
    if(fs.existsSync(folder_image)){
    	//clearFiles(folder_image);
    }else{
	    fs.mkdirSync(folder_image);
	}
} catch (e) {
}

ntv_report_msg("DEVICE PROBER START...");
if (args.ipaddress) {
    // Connection Details and IP Address supplied in the Command Line
    IPADDRESS = args.ipaddress;
    if (args.port) PORT = parseInt(args.port);
    if (args.username) USERNAME = args.username;
    if (args.password) PASSWORD = args.password;


    // Perform an Audit of all the cameras in the IP address Range
    perform_audit(IPADDRESS, PORT, USERNAME, PASSWORD);
}

if (args.filename) {
    // Connection details supplied in a .JSON file
    let contents = fs.readFileSync(args.filename);
    let file = JSON.parse(contents);

    if (file.cameralist && file.cameralist.length > 0) {
        // process each item in the camera list
        //Note - forEach is asynchronous - you don't know when it has completed
        file.cameralist.forEach(function (item) {
            // check IP range start and end
            if (item.ipaddress) IPADDRESS = item.ipaddress;
            if (item.port) PORT = item.port;
            if (item.username) USERNAME = item.username;
            if (item.password) PASSWORD = item.password;

            perform_audit(IPADDRESS, PORT, USERNAME, PASSWORD);
        }
        );
    }
}

if (args.scan) {
    // set up an event handler which is called for each device discovered
    onvif.Discovery.on('device', function(cam,rinfo,xml){
        // function will be called as soon as NVT responses
        parseString(xml, 
            {
                tagNameProcessors: [ stripPrefix ]   // strip namespace eg tt:Data -> Data
            },
            function (err, result) {
                if (err) return;
                let xaddrs = result['Envelope']['Body'][0]['ProbeMatches'][0]['ProbeMatch'][0]['XAddrs'][0];
                let scopes = result['Envelope']['Body'][0]['ProbeMatches'][0]['ProbeMatch'][0]['Scopes'][0];
                scopes = scopes.split(" ");

                let hardare = "";
                let name = "";
                for (let i = 0; i < scopes.length; i++) {
                    // use decodeUri to conver %20 to ' '
                    if (scopes[i].includes('onvif://www.onvif.org/name')) name = decodeURI(scopes[i].substring(27));
                    if (scopes[i].includes('onvif://www.onvif.org/hardware')) hardware = decodeURI(scopes[i].substring(31));
                }
                // split scopes on Space
                let msg = 'Discovery Reply from ' + rinfo.address + ' (' + name + ') (' + hardware + ')';
                //console.log('%j',result);
                console.log(msg);
            }
        );

    })

    // start the probe
    // resolve=false  means Do not create Cam objects
    onvif.Discovery.probe({resolve: false});
}

// program ends here (just functions below)

/*
 * code=0  OK
 * code=403 needs login
 * code=1   ip format error
 */
function postCall(postUrl,data) {
	var content  = '' + data;
	var parseObj = url.parse(postUrl);
	
	let request;
	let defaultPort;
	if(postUrl.indexOf("https")>=0){
		request  = https;
		defaultPort = 443;
	}else{
		request  = http;
		defaultPort = 80;
	}
	//console.log(parseObj.hostname + ":" + parseObj.port);
	var options = {
			hostname: parseObj.hostname,
			port:     parseObj.port, //?parseObj.port:defaultPort,
			path:     parseObj.path,
			method:   'POST',
			timeout:  1000,
			headers: {
				'Content-Type': 'text/plain',
				'Content-Length': content.length
			}
		};
	var req = request.request(options, function(res) {
			res.setEncoding('utf8');
			res.on('data', function(chunk) {
				//console.log(chunk);
			});
			res.on('end', () => {
				
			});
		});
		
	req.on('error', function(e) {
		console.log('PROBLE_ERROR:post_data_error');
	});
	// write data to request body  
	req.write(content);
	req.end();
}

//发送过程消息给侦听端
function ntv_report_msg(msg){
	let onvifObj  = new Object();
    onvifObj.host = "-";
    onvifObj.code = 200;
    onvifObj.error= msg;
    if(postUrl){
		postCall(postUrl,JSON.stringify(onvifObj));
	}
}

function ntv_callback(ip,data){
	let log_filename = folder_report + path.sep + ip + '.inf';
	let log_fd;
	
	//console.log(data);
	if(ip!=""){
		fs.open(log_filename, 'w', function(err, fd) {
			if(err) {
				console.log('PROBLE_ERROR:open_report_file_failed');
				return;
			}
			log_fd = fd;
		
			fs.write(log_fd, data, function(err) {
				if(err)
					console.log('PROBLE_ERROR:write_report_failed');
			});
			//fs.close(fd);
		});
	}
	
	if(postUrl){
		postCall(postUrl,data);
	}
}

function perform_audit(ip_addresses, port, username, password) {
    let ip_list = [];

    // Valid IP addresses are
    // a) Single address 1.2.3.4
    // b) Range 10.10.10.50-10.10.10.99
    // c) List 1.1.1.1,2.2.2.2,3.3.3.3
    // d) Mixture 1.2.3.4,10.10.10.50-10.10.10.99

    ip_addresses = ip_addresses.split(',');
    for (let i = 0; i < ip_addresses.length; i++) {
        let item = ip_addresses[i];
        if (item.includes('-')) {
            // item contains '-'. Split on the '-'
            let split_str = item.split('-');
            if (split_str.length != 2) {
                console.log('PROBLE_ERROR:IP_RANGE_FORMAT_IS_ERROR');
                
                let onvifObj  = new Object();
                	onvifObj.host = "-";
                	onvifObj.code = 1;
                	onvifObj.error= "ip format error";
                ntv_callback("",JSON.stringify(onvifObj));

				 process.exit(1);
            }
            let ip_start = split_str[0];
            let ip_end = split_str[1];
            if(!check_range(ip_start,ip_end)){
            	console.log('PROBLE_ERROR:IP_RANGE_FORMAT_IS_ERROR2');
            	let onvifObj  = new Object();
                	onvifObj.host = "-";
                	onvifObj.code = 1;
                	onvifObj.error= "ip format error";
                ntv_callback("",JSON.stringify(onvifObj));
                
                process.exit(1);
            }

            let tmp_list = generate_range(ip_start, ip_end);

            // Copy
            for (let x = 0; x < tmp_list.length; x++) ip_list.push(tmp_list[x]);
        }
        else {
            // item does not include a '-' symbol
            if(!isValidIP(item)){
            	console.log('PROBLE_ERROR:IP_RANGE_FORMAT_IS_ERROR3');
            	process.exit(1);
            }
            ip_list.push(item);
        }
    }

    // hide error messages
    console.error = function () { };

    // try each IP address and each Port
    ip_list.forEach(function (ip_entry) {

        //console.log('scan ' + ip_entry + ':' + port + ' ...' + gtimeout);
        new Cam({
            hostname: ip_entry,
            username: username,
            password: password,
            port: port,
            timeout: gtimeout
        }, function CamFunc(err) {
            if (err) {
                console.log("ONVIF_ERROR:"  + ip_entry + " " + err);
                var errStr = "" + err;
                
                //Soup响应特征
                if(errStr.indexOf("ONVIF")>0 && errStr.indexOf("Fault")>0){
                	let onvifObj  = new Object();
                	onvifObj.host = ip_entry + ':' + port;
                	onvifObj.code = 403;
                	onvifObj.error= errStr;
                	return ntv_callback(ip_entry,JSON.stringify(onvifObj));
                }
                
                return;
                
            }

            let cam_obj = this;

            let got_date;
            let got_info;
            let got_snapshots = [];
            let got_live_stream_tcp;
            let got_live_stream_udp;
            let got_live_stream_multicast;
            let got_recordings;

            // Use Nimble to execute each ONVIF function in turn
            // This is used so we can wait on all ONVIF replies before
            // writing to the console
            flow.series([
                function (nimble_callback) {
                    cam_obj.getSystemDateAndTime(function (err, date, xml) {
                        if (!err) got_date = date;
                        nimble_callback();
                    });
                },
                function (nimble_callback) {
                    cam_obj.getDeviceInformation(function (err, info, xml) {
                        if (!err) got_info = info;
                        nimble_callback();
                    });
                },
                function (nimble_callback) {
                    try {
                        // The ONVIF device may have multiple Video Sources
                        // eg 4 channel IP encoder or Panoramic Cameras
                        // Grab a JPEG from each VideoSource
                        // Note. The Nimble Callback is only called once we have ONVIF replies
                        // have been returned
                        let reply_max = cam_obj.activeSources.length;
                        let reply_count = 0;
                        for (let src_idx = 0; src_idx < cam_obj.activeSources.length; src_idx++) {
                            let videoSource = cam_obj.activeSources[src_idx];
                            cam_obj.getSnapshotUri({profileToken: videoSource.profileToken},function (err, getUri_result, xml) {
                                reply_count++;
                                if (!err) got_snapshots.push(getUri_result);

                                const http = require('http');
                                const fs = require('fs');
                                const url = require('url');
                                const request = require('request');

                                let filename = "";
                                if (cam_obj.activeSources.length === 1) {
                                    filename = folder_image + path.sep + 'snapshot_' + ip_entry + '.jpg';
                                } else {
                                    // add _1, _2, _3 etc for cameras with multiple VideoSources
                                    filename = folder_image + path.sep + 'snapshot_' + ip_entry + '_' + (src_idx+1) + '.jpg';
                                }
                                let uri = url.parse(getUri_result.uri);

                                // handle the case where the camera is behind NAT
                                // ONVIF Standard now says use XAddr for camera
                                // and ignore the IP address in the Snapshot URI
                                uri.host = ip_entry;
                                uri.username = username;
                                uri.password = password;
                                if (!uri.port) uri.port = 80;
                                let modified_uri = uri.href;

                                let digestRequest = require('request-digest')(username, password);
                                digestRequest.request({
                                    host: 'http://' + uri.host,
                                    path: uri.path,
                                    port: uri.port,
                                    encoding: null, // return data as a Buffer()
                                    method: 'GET'
                                    //                             headers: {
                                    //                               'Custom-Header': 'OneValue',
                                    //                               'Other-Custom-Header': 'OtherValue'
                                    //                             }
                                }, function (error, response, body) {
                                    if (error) {
                                        // console.log('Error downloading snapshot');
                                        // throw error;
                                    } else {
                                    	/*
                                    	 * fs.open 确保打开文件并将原有内容抹除（如果有）。
                                    	 * fs.appendFile 追加数据。
                                    	 * 实际上有逻辑错误，如果返回数据的回调有多次调用，始终只写入一次数据。
                                    	 * 目前来看只有一次回调。有问题的话再改进吧。
                                    	 * ----WJ
                                    	 */
                                        fs.open(filename, 'w', function (err, fd) {
                                            // callback for file opened, or file open error
                                            if (err) {
                                                console.log('PROBLE_ERROR:open_image_file_failed ');
                                                return;
                                            }
                                            
                                            fs.appendFile(filename, body, function (err) {
                                                if (err) {
                                                    console.log('PROBLE_ERROR:write_image_file_failed');
                                                }
                                            });
                                        });
                                    }
                                });

                                if (reply_count === reply_max) nimble_callback(); // let 'flow' move on. JPEG GET is still async
                            });
                        }; // end for
                    } catch (err) { nimble_callback(); }
                },
                function (nimble_callback) {
                    try {
                        cam_obj.getStreamUri({
                            protocol: 'RTSP',
                            stream: 'RTP-Unicast'
                        }, function (err, stream, xml) {
                            if (!err) got_live_stream_tcp = stream;
                            nimble_callback();
                        });
                    } catch (err) { nimble_callback(); }
                },
                function (nimble_callback) {
                    try {
                        cam_obj.getStreamUri({
                            protocol: 'UDP',
                            stream: 'RTP-Unicast'
                        }, function (err, stream, xml) {
                            if (!err) got_live_stream_udp = stream;
                            nimble_callback();
                        });
                    } catch (err) { nimble_callback(); }
                },
                /* Multicast is optional in Profile S, Mandatory in Profile T
                but could be disabled
                function (nimble_callback) {
                    try {
                        cam_obj.getStreamUri({
                            protocol: 'UDP',
                            stream: 'RTP-Multicast'
                        }, function (err, stream, xml) {
                            if (!err) got_live_stream_multicast = stream;
                            nimble_callback();
                        });
                    } catch (err) { nimble_callback(); }
                },
                */
                function (nimble_callback) {
                    let str = "";
                	let msg = "";
                	let lend= "\r\n";
                	let onvifObj = new Object();
                	
                	str = 'PROBLE_OK:' +  ip_entry + lend;
                	msg += str;
                	
                	str =  'host=' + ip_entry + ':' + port + lend;
                	msg += str;
                	onvifObj.host = ip_entry + ':' + port;
                	onvifObj.code = 0;
                	
                	if(got_date) {
                		str= 'date=' + got_date + lend;
                		msg += str;
                		onvifObj.date = got_date;
                	}
                	if(got_info) {
                		str= 'manufacturer='     + got_info.manufacturer    + lend;
                		msg += str;
                		onvifObj.manufacturer = got_info.manufacturer;
                		
                		str= 'model='            + got_info.model           + lend;
                		msg += str;
                		onvifObj.model = got_info.model;
                		
                		str= 'firmwareVersion=' + got_info.firmwareVersion + lend;
                		msg += str;
                		onvifObj.firmwareVersion = got_info.firmwareVersion;
                		
                		str= 'serialNumber='    + got_info.serialNumber    + lend;
                		msg += str;
                		onvifObj.serialNumber = got_info.serialNumber;
                		
                		str= 'hardwareId='      + got_info.hardwareId      + lend;
                		msg += str;
                		onvifObj.hardwareId = got_info.hardwareId;
                	}
                	if(got_snapshots.length > 0) {
                		for(let i = 0; i < got_snapshots.length; i++) {
                			str = 'snapshot=' + got_snapshots[i].uri + lend
                			msg += str;
                		}
                		onvifObj.snapshots = got_snapshots;
                	}
                	if(got_live_stream_tcp) {
                		str= 'tcp_stream=' + got_live_stream_tcp.uri + lend;
                		msg += str;
                		onvifObj.tcp_stream = got_live_stream_tcp;
                	}
                	if(got_live_stream_udp) {
                		str= 'udp_stream=' + got_live_stream_udp.uri + lend;
                		msg += str;
                		onvifObj.udp_stream = got_live_stream_udp;
                	}
                	//返回的流信息（meta）就是上述取到的直播流的信息，见说明文档
                	onvifObj.activeSource = cam_obj.activeSource;
                    
                    str = '------------------------------------------------' + lend;
                	msg += str;
                	
                    console.log(msg);
                    //console.log(JSON.stringify(onvifObj));
                    ntv_callback(ip_entry,JSON.stringify(onvifObj));
                    
                    nimble_callback();
                },

            ]); // end flow

        });
    }); // foreach
}

function clearFiles(folder){
    let files = [];
    if(fs.existsSync(folder)){
        files = fs.readdirSync(folder);
        files.forEach((filename, index) => {
            let curPath = folder + path.sep + filename;
            if(fs.statSync(curPath).isDirectory()){
                //
            } else {
                fs.unlinkSync(curPath); //删除文件
            }
        });
    }
}

function isValidIP(ip) {
    var reg = /^(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])$/
    return reg.test(ip);
} 

function check_range(start_ip, end_ip){
	if(!isValidIP(start_ip) || !isValidIP(end_ip)){
		return false;
	}
	
	var index1 = start_ip.lastIndexOf('.');
	var index2 = end_ip.lastIndexOf('.');
	if(start_ip.substr(0,index1)!=end_ip.substr(0,index1)){
		return false;
	}
	
	return true;
}

function generate_range(start_ip, end_ip) {
    let start_long = toLong(start_ip);
    let end_long = toLong(end_ip);
    if (start_long > end_long) {
        let tmp = start_long;
        start_long = end_long
        end_long = tmp;
    }
    let range_array = [];
    let i;
    for (let i = start_long; i <= end_long; i++) {
        range_array.push(fromLong(i));
    }
    return range_array;
}

//toLong taken from NPM package 'ip' 
function toLong(ip) {
    let ipl = 0;
    ip.split('.').forEach(function (octet) {
        ipl <<= 8;
        ipl += parseInt(octet);
    });
    return (ipl >>> 0);
};

//fromLong taken from NPM package 'ip' 
function fromLong(ipl) {
    return ((ipl >>> 24) + '.' +
        (ipl >> 16 & 255) + '.' +
        (ipl >> 8 & 255) + '.' +
        (ipl & 255));
};

