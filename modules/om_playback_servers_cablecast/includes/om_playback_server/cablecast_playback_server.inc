<?php
class cablecastPlaybackServerController extends playbackServerController {
  /**
   * First few methods come from the playbackServerController interface.
   * --Methods dealing with metadata transmission.
   */
  public function create_metadata($pbs, $metadata = array()) {
    if (is_object($metadata)) {
      $metadata = array($metadata);
    }
    foreach ($metadata AS $show) {
      $reel = $this->create_reel($pbs, $show);
      // Map drupal fields to cablecast fields.
      $params = array (
        'LocalID'       => '', //string</LocalID>
        'Title'         => $show->title,
        'CGTitle'       => $show->title, //string</CGTitle>
        'ProjectID'     => 0, //int</ProjectID>
        'CGExempt'      => 'false', //boolean</CGExempt>
        'ProducerID'    => 0, //int</ProducerID>
        'CategoryID'    => 0, //int</CategoryID>
        'EventDate'     => date('Y-m-d\TH:i:s', $show->created), //dateTime</EventDate>
        'Comments'      => '',
        'CustomFields'  => array(),
        'OnDemand'  => FALSE,
        'OnDemandStatus'  => '',
        'CrawlLengthInSeconds' => 0,
        'CrawlText' => '',
        'BugText' => '',
        'Reels'=> $reel,
        'LocationID' => $pbs->location_id,
      );
      if (isset($show->field_archive_transfer[$show->language]) && $show->field_archive_transfer[$show->language][0]['value'] == 1) {
        $params['CustomFields'] = array(
          'CustomField' => array(
            'Name' => 'Archive-Upload',
            'Value' => 'x'
          )
        );
      }
      return $this->soap_request($pbs, 'CreateNewShowRecord', $params);
    }
  }
  public function check_metadata_status($pbs, $metadata = array()) {
    $shows = array();
    $output = array();
    if (is_numeric($metadata)) {
      $metadata = array($metadata);
    }
    foreach($metadata AS $id) {
      $response = $this->soap_request($pbs, 'GetShowInformation', array('ShowID' => $id));
      if (is_object($response->GetShowInformationResult)) {
        $shows[] = (array)$response->GetShowInformationResult;
      }
    }
    if (count($shows) == 1) {
      return array_shift($shows);
    }
    return $shows;
  }
  public function update_metadata($pbs, $metadata = array()) {
  }
  public function delete_metadata($pbs, $metadata = array()) {
  }
  /**
   * Use a uri to return a show record.
   */
  public function metadata_record_from_uri($pbs, $uri) {
    if (class_exists('cablecastFile')) {
      $file_controller = new cablecastFile();
      $record = $file_controller->get_show_record($uri);
      if (!empty($record)) {
        return $record;
      }
    }
    return FALSE;
  }
  /**
   * --Methods dealing with files
   */
  public function check_file_status($pbs, stdClass $show) {
    $lang = $show->language;
    $id = $show->field_om_show_playback_server_id[$lang][0]['value'];
    $params = array(
      'LocationID' => $pbs->location_id,
      'IncludeValidFiles' => TRUE,
      'IncludeInvalidFiles' => TRUE,
      'RestrictToShowID' => $id
    );
    return $this->soap_request($pbs, 'GetDigitalFiles', $params);
  }
  
  /**
   * --Methods dealing with airings.
   */
  public function create_airing($pbs, $schedule_info = array()) {
    $params = array(
      'ChannelID'   => $pbs->output, //int</ChannelID>
      'LocationID'  => $pbs->location_id, //int</LocationID>
      'ShowID'      => $schedule_info['ShowID'], //int</ShowID>
      'RunDateTime' => date('Y-m-d\TH:i:s', $schedule_info['start']), //dateTime</RunDateTime>
      'RunLock'     => TRUE, //boolean</RunLock> 
      'RunBump'     => 0, //int</RunBump>
      'CGExempt'    => FALSE, //boolean</CGExempt>
      'BugText'     => '', //string</BugText>
      'CrawlText'   => '', //string</CrawlText>
      'CrawlLength' => 0, //int</CrawlLength>
    );
    $response = $this->soap_request($pbs, 'AddScheduleEvent', $params);
    if (isset($response->AddScheduleEventResult)) {
      return array('ScheduleID' => $response->AddScheduleEventResult);
    }
    return FALSE;
  }
  public function update_airing($pbs, $schedule_info = array()) {
    
  }
  public function get_airing($pbs, $schedule_info = array()) {
    if (!empty($schedule_info) && class_exists('cablecastFile')) {
      $params = array (
        'ChannelID' => $pbs->output,
        'FromDate' => date("Y-m-d\TH:i:s", $schedule_info['start']),
        'ToDate' => date("Y-m-d\TH:i:s", $schedule_info['end']),
        'restrictToShowID' => 0
      );
      $request = $this->soap_request($pbs, 'GetScheduleInformation', $params);
      $airings = array();
      if (!empty($request->GetScheduleInformationResult->ScheduleInfo)) {
        if (is_object($request->GetScheduleInformationResult->ScheduleInfo)) {
          $request->GetScheduleInformationResult->ScheduleInfo = array($request->GetScheduleInformationResult->ScheduleInfo);
        }
        $file_controller = new cablecastFile();
        foreach ($request->GetScheduleInformationResult->ScheduleInfo AS $airing) {
          $uri = 'cablecast://' . $airing->ShowID;
          $entity_id = $file_controller->get_entity_id_from_uri($uri);
          $airings[] = array(
            'uid' => $airing->ScheduleID,
            'start' => strtotime($airing->StartTime),
            'end' => strtotime($airing->EndTime),
            'show_id' => $entity_id
           );
        }
      }
      return $airings;
    }
    else {
      throw new Exception('Cablecast Playback Server: received empty schedule information');
    }
  }
  public function delete_airing($pbs, $schedule_info = array()) {
    
  }
  /**
   * --Methods dealing with system health
   */
  public function get_server_health($pbs) {
    return $this->soap_request($pbs, 'GetChannels');
  }
  // Custom and helper methods
  /**
   * Make a soap request to the pbs provided.
   */
  private function soap_request($pbs, $function, $data = array()) {
    try {
      $endpoint = $pbs->uri . '/CablecastWS/CablecastWS.asmx?WSDL';
      if (!file_exists('/tmp/cablecast.wsdl')) {
        file_put_contents("/tmp/cablecast.wsdl", file_get_contents($endpoint));
      }
      $client = new SoapClient('/tmp/cablecast.wsdl');
      $auth = array(
        'username' => $pbs->user,
        'password' => $pbs->password
      );
      $data = array_merge($data, $auth);
      return $client->{$function}($data);
    }
    catch(Exception $e) {
      if (is_object($pbs)) {
        $message = 'During function ' . $function . 'Cablecast Playback Server: ' . $pbs->name . ' threw exception: ' . $e->getMessage();
      }
      else {
        $message =  'Cablecast Playback Server: malformed playback server entity: ' . $e->getMessage();
      }
      throw new Exception($message);
    }
  }
  private function create_reel($pbs, $file) {
    $format = 0;
    $duration = 0;
    $lang = $file->language;
    
    $available_formats = variable_get('cablecast_available_formats_' . $pbs->pbsid, array());
    if (empty($available_formats)) {
      $all_formats = $this->get_formats($pbs);
      if (!empty($all_formats)) {
        foreach ($all_formats->GetFormatsResult->Format AS $possible_format) {
          if ($possible_format->Name == 'SD Digital File' || $possible_format->Name == 'HD Digital File') {
            $available_formats[$possible_format->Name] = $possible_format->FormatID;
          }
        }
      }
      variable_set('cablecast_available_formats_' . $pbs->pbsid, $available_formats);
    }
    if (isset($file->field_om_show_media_format[$lang][0]['value'])) {
      if ($file->field_om_show_media_format[$lang][0]['value'] == 'hd') {
        $format = $available_formats['HD Digital File'];
      }
      if ($file->field_om_show_media_format[$lang][0]['value'] == 'sd') {
        $format = $available_formats['SD Digital File'];
      }
    }
    if (isset($file->field_om_show_duration[$lang][0]['value'])) {
      $duration = $file->field_om_show_duration[$lang][0]['value'];
    }
    return array(
      'NewReel' => array(
        'FormatID'=> $format, //int</FormatID>
        'CueSeconds'=> (int)$duration , //int</LengthSeconds>
        'LengthSeconds'=> 0 , //int</LengthSeconds>
        'Title'=> 0, //int</Title>
        'Chapter'=> 0, //int</Chapter>
      ),
    );
  }
  /**
   * Get a list of all formats from the pbs location.
   */
  public function get_formats($pbs) {
    return $this->soap_request($pbs, 'GetFormats', array('LocationID' => $pbs->location_id));
  }
  
  /**
   * Get a list of all files with filter criteria.
   */
  public function get_digital_files($pbs, $options = array()) {
    $params = array(
      'LocationID' => $pbs->location_id,
      'IncludeValidFiles' => TRUE,
      'IncludeInvalidFiles' => FALSE,
      'RestrictToOrphanedFiles' => TRUE,
      'RestrictToShowID' => 0
    );
    $params = array_merge($params, $options);
    return $this->soap_request($pbs, 'GetDigitalFiles', $params);
  }
  /**
   * Rename a digital file.
   */
  public function rename_file($pbs, $new_file = array()) {
    return $this->soap_request($pbs, 'RenameDigitalFile', $new_file);
  }
  /**
   * Send autopilot.
   */
  public function send_autopilot($pbs, $options) {
    $params = array(
      'LocationID' => $pbs->location_id,
      'endDateTime' => date('Y-m-d\TH:i:s'),
      'StickyDevices' => FALSE,
      'ClearBumps' => FALSE,
      'SendEventsToHardware' => FALSE
    );
    $params = array_merge($params, $options);
    return $this->soap_request($pbs, 'SendAutopilot', $params);
  }
} 